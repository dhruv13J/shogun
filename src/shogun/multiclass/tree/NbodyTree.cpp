/*
 * Copyright (c) The Shogun Machine Learning Toolbox
 * Written (w) 2014 Parijat Mazumdar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of the Shogun Development Team.
 */

#include <shogun/multiclass/tree/NbodyTree.h>
#include <shogun/distributions/KernelDensity.h> 

using namespace shogun;

CNbodyTree::CNbodyTree(int32_t leaf_size, EDistanceType d)
: CTreeMachine<NbodyTreeNodeData>()
{
	init();

	m_leaf_size=leaf_size;
	m_dist=d;
}

void CNbodyTree::build_tree(CDenseFeatures<float64_t>* data)
{
	REQUIRE(data,"data not set\n");
	REQUIRE(m_leaf_size>0,"Leaf size should be greater than 0\n");

	knn_done=false;
	m_data=data->get_feature_matrix();

	vec_id=SGVector<index_t>(m_data.num_cols);
	vec_id.range_fill(0);

	set_root(recursive_build(0,m_data.num_cols-1));
}

void CNbodyTree::query_knn(CDenseFeatures<float64_t>* data, int32_t k)
{
	REQUIRE(data,"Query data not supplied\n")
	REQUIRE(data->get_num_features()==m_data.num_rows,"query data dimension should be same as training data dimension\n")

	knn_done=true;
	SGMatrix<float64_t> qfeats=data->get_feature_matrix();
	knn_dists=SGMatrix<float64_t>(k,qfeats.num_cols);
	knn_indices=SGMatrix<index_t>(k,qfeats.num_cols);
	int32_t dim=qfeats.num_rows;

	for (int32_t i=0;i<qfeats.num_cols;i++)
	{
		CKNNHeap* heap=new CKNNHeap(k);
		bnode_t* root=NULL;
		if (m_root)
			root=dynamic_cast<bnode_t*>(m_root);

		float64_t mdist=min_dist(root,qfeats.matrix+i*dim,dim);
		query_knn_single(heap,mdist,root,qfeats.matrix+i*dim,dim);
		memcpy(knn_dists.matrix+i*k,heap->get_dists(),k*sizeof(float64_t));
		memcpy(knn_indices.matrix+i*k,heap->get_indices(),k*sizeof(index_t));		

		delete(heap);
	}
}

SGVector<float64_t> CNbodyTree::log_kernel_density(SGMatrix<float64_t> test, EKernelType kernel, float64_t h, float64_t atol, float64_t rtol)
{
	int32_t dim=m_data.num_rows;
	REQUIRE(test.num_rows==dim,"dimensions of training data and test data should be the same\n")

	float64_t log_atol=CMath::log(atol*m_data.num_cols);
	float64_t log_rtol=CMath::log(rtol);
	float64_t log_kernel_norm=CKernelDensity::log_norm(kernel,h,dim);
	SGVector<float64_t> log_density(test.num_cols);
	for (int32_t i=0;i<test.num_cols;i++)
	{
		bnode_t* root=NULL;
		if (m_root)
			root=dynamic_cast<bnode_t*>(m_root);

		float64_t lower_dist=0;
		float64_t upper_dist=0;
		min_max_dist(test.matrix+i*dim,root,lower_dist,upper_dist,dim);

		float64_t min_bound=CMath::log(m_data.num_cols)+CKernelDensity::log_kernel(kernel,upper_dist,h);
		float64_t max_bound=CMath::log(m_data.num_cols)+CKernelDensity::log_kernel(kernel,lower_dist,h);
		float64_t spread=logdiffexp(max_bound,min_bound);

		get_kde_single(root,test.matrix+i*dim,kernel,h,log_atol,log_rtol,log_kernel_norm,min_bound,spread,min_bound,spread);
		log_density[i]=logsumexp(min_bound,spread-CMath::log(2))+log_kernel_norm-CMath::log(m_data.num_cols);
	}

	return log_density;
}

SGMatrix<float64_t> CNbodyTree::get_knn_dists()
{
	if (knn_done)
		return knn_dists;

	SG_ERROR("knn query has not been executed yet\n");
	return SGMatrix<float64_t>();
}

SGMatrix<index_t> CNbodyTree::get_knn_indices()
{
	if (knn_done)
		return knn_indices;

	SG_ERROR("knn query has not been executed yet\n");
	return SGMatrix<index_t>();
}

void CNbodyTree::query_knn_single(CKNNHeap* heap, float64_t mdist, bnode_t* node, float64_t* arr, int32_t dim)
{
	if (mdist>heap->get_max_dist())
		return;

	if (node->data.is_leaf)
	{
		index_t start=node->data.start_idx;
		index_t end=node->data.end_idx;

		for (int32_t i=start;i<=end;i++)
			heap->push(vec_id[i],distance(vec_id[i],arr,dim));

		return;
	}

	bnode_t* cleft=node->left();
	bnode_t* cright=node->right();

	float64_t min_dist_left=min_dist(cleft,arr,dim);
	float64_t min_dist_right=min_dist(cright,arr,dim);

	if (min_dist_left<=min_dist_right)
	{
		query_knn_single(heap,min_dist_left,cleft,arr,dim);
		query_knn_single(heap,min_dist_right,cright,arr,dim);		
	}
	else
	{
		query_knn_single(heap,min_dist_right,cright,arr,dim);		
		query_knn_single(heap,min_dist_left,cleft,arr,dim);		
	}

	SG_UNREF(cleft);
	SG_UNREF(cright);
}

float64_t CNbodyTree::distance(index_t vec, float64_t* arr, int32_t dim)
{
	float64_t ret=0;
	for (int32_t i=0;i<dim;i++)
		ret+=add_dim_dist(m_data(i,vec)-arr[i]);

	return actual_dists(ret);
}

CBinaryTreeMachineNode<NbodyTreeNodeData>* CNbodyTree::recursive_build(index_t start, index_t end)
{
	bnode_t* node=new bnode_t();
	init_node(node,start,end);

	// stopping critertia
	if (end-start+1<m_leaf_size*2)
	{
		node->data.is_leaf=true;
		return node;
	}

	node->data.is_leaf=false;
	index_t dim=find_split_dim(node);
	index_t mid=(end+start)/2;
	partition(dim,start,end,mid);

	bnode_t* child_left=recursive_build(start,mid);
	bnode_t* child_right=recursive_build(mid+1,end);

	node->left(child_left);
	node->right(child_right);

	return node;
}

void CNbodyTree::get_kde_single(bnode_t* node,float64_t* data, EKernelType kernel, float64_t h, float64_t log_atol, float64_t log_rtol,
	float64_t log_norm, float64_t min_bound_node, float64_t spread_node, float64_t &min_bound_global, float64_t &spread_global)
{
	int32_t n_node=CMath::log(node->data.end_idx-node->data.start_idx+1);
	int32_t n_total=CMath::log(m_data.num_cols);

	// local bound criterion met
	if ((log_norm+spread_node+n_total-n_node)<=logsumexp(log_atol,log_rtol+log_norm+min_bound_node))
		return;

	// global bound criterion met	
	if ((log_norm+spread_global)<=logsumexp(log_atol,log_rtol+log_norm+min_bound_global))
		return;

	// node is leaf
	if (node->data.is_leaf)
	{
		min_bound_global=logdiffexp(min_bound_global,min_bound_node);
		spread_global=logdiffexp(spread_global,spread_node);

		for (int32_t i=node->data.start_idx;i<=node->data.end_idx;i++)
		{
			float64_t pt_eval=CKernelDensity::log_kernel(kernel,distance(vec_id[i],data,m_data.num_rows),h);
			min_bound_global=logsumexp(pt_eval,min_bound_global);
		}

		return;
	}

	bnode_t* lchild=node->left();
	bnode_t* rchild=node->right();

	float64_t lower_dist=0;
	float64_t upper_dist=0;
	min_max_dist(data,lchild,lower_dist,upper_dist,m_data.num_rows);

	int32_t n_l=lchild->data.end_idx-lchild->data.start_idx+1;
	float64_t lower_bound_childl=log(n_l)+CKernelDensity::log_kernel(kernel,upper_dist,h);
	float64_t spread_childl=logdiffexp(log(n_l)+CKernelDensity::log_kernel(kernel,lower_dist,h),lower_bound_childl);

	min_max_dist(data,rchild,lower_dist,upper_dist,m_data.num_rows);
	int32_t n_r=rchild->data.end_idx-rchild->data.start_idx+1;
	float64_t lower_bound_childr=log(n_r)+CKernelDensity::log_kernel(kernel,upper_dist,h);
	float64_t spread_childr=logdiffexp(log(n_r)+CKernelDensity::log_kernel(kernel,lower_dist,h),lower_bound_childr);

	// update global bounds
	min_bound_global=logdiffexp(min_bound_global,min_bound_node);
	min_bound_global=logsumexp(min_bound_global,lower_bound_childl);
	min_bound_global=logsumexp(min_bound_global,lower_bound_childr);

	spread_global=logdiffexp(spread_global,spread_node);
	spread_global=logsumexp(spread_global,spread_childl);
	spread_global=logsumexp(spread_global,spread_childr);

	get_kde_single(lchild,data,kernel,h,log_atol,log_rtol,log_norm,lower_bound_childl,spread_childl,min_bound_global,spread_global);
	get_kde_single(rchild,data,kernel,h,log_atol,log_rtol,log_norm,lower_bound_childr,spread_childr,min_bound_global,spread_global);

	SG_UNREF(lchild);
	SG_UNREF(rchild);
}

void CNbodyTree::partition(index_t dim, index_t start, index_t end, index_t mid)
{
	// in-place partial quick-sort
	index_t left=start;
	index_t right=end;
	while (true)
	{
		index_t midindex=left;
		for (int32_t i=left;i<right;i++)
		{
			if (m_data(dim,vec_id[i])<m_data(dim,vec_id[right]))
			{
				CMath::swap(*(vec_id.vector+i),*(vec_id.vector+midindex));
				midindex+=1;
			}
		}

		CMath::swap(*(vec_id.vector+midindex),*(vec_id.vector+right));
		if (midindex==mid)
			break;
		else if (midindex<mid)
			left=midindex+1;
		else
			right=midindex-1;
	}
}

index_t CNbodyTree::find_split_dim(bnode_t* node)
{
	SGVector<float64_t> upper_bounds=node->data.bbox_upper;
	SGVector<float64_t> lower_bounds=node->data.bbox_lower;

	index_t max_dim=0;
	float64_t max_spread=-1;
	for (int32_t i=0;i<m_data.num_rows;i++)
	{
		float64_t spread=upper_bounds[i]-lower_bounds[i];
		if (spread>max_spread)
		{
			max_spread=spread;
			max_dim=i;
		}
	}

	return max_dim;
}

void CNbodyTree::init()
{
	m_data=SGMatrix<float64_t>();
	m_leaf_size=1;
	vec_id=SGVector<index_t>();
	m_dist=D_EUCLIDEAN;
	knn_done=false;
	knn_dists=SGMatrix<float64_t>();
	knn_indices=SGMatrix<index_t>();	

	SG_ADD(&m_data,"m_data","data matrix",MS_NOT_AVAILABLE);
	SG_ADD(&m_leaf_size,"m_leaf_size","leaf size",MS_NOT_AVAILABLE);
	SG_ADD(&vec_id,"vec_id","id of vectors",MS_NOT_AVAILABLE);
	SG_ADD(&knn_done,"knn_done","knn done or not",MS_NOT_AVAILABLE);
	SG_ADD(&knn_dists,"knn_dists","knn distances",MS_NOT_AVAILABLE);
	SG_ADD(&knn_indices,"knn_indices","knn indices",MS_NOT_AVAILABLE);					
}