FILE(STRINGS "${CMAKE_SOURCE_DIR}/NEWS" NEWS LIMIT_COUNT 5)
STRING(REGEX REPLACE ".*SHOGUN Release version ([0-9.]*).*" "\\1" VERSION "${NEWS}")
STRING(REGEX REPLACE ".*SHOGUN Release version.*parameter ([0-9]+).*" "\\1" VERSION_PARAMETER "${NEWS}")
SET(MAINVERSION ${VERSION})

SET(EXTRA "")

IF(EXISTS "${CMAKE_SOURCE_DIR}/.git/")
	FIND_PACKAGE(Git QUIET)
	IF (NOT GIT_FOUND)
		MESSAGE(FATAL_ERROR "The source is checked out from a git repository, but cannot find git executable!")
	ENDIF()


	EXECUTE_PROCESS(
		COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
		OUTPUT_VARIABLE BRANCH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	EXECUTE_PROCESS(
		COMMAND ${GIT_EXECUTABLE} log -1 --pretty=%h
		OUTPUT_VARIABLE REVISION
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)


	EXECUTE_PROCESS(
		COMMAND ${GIT_EXECUTABLE} merge-base ${BRANCH} HEAD
		OUTPUT_VARIABLE BRANCH_POINT
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	EXECUTE_PROCESS(
		COMMAND ${GIT_EXECUTABLE} log -1 --pretty=%ai ${BRANCH_POINT}
		OUTPUT_VARIABLE DATEINFO
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	STRING(REGEX REPLACE "([0-9]+)-.*" "\\1" VERSION_YEAR "${DATEINFO}")
	STRING(REGEX REPLACE ".*-0*([0-9]+)-.*" "\\1" VERSION_MONTH "${DATEINFO}")
	STRING(REGEX REPLACE ".*-.*-0*([0-9]+).*" "\\1" VERSION_DAY "${DATEINFO}")
	STRING(REGEX REPLACE ".* 0*([0-9]+):.*" "\\1" VERSION_HOUR "${DATEINFO}")
	STRING(REGEX REPLACE ".*:0*([0-9]+):.*" "\\1" VERSION_MINUTE "${DATEINFO}")

	SET(REVISION_PREFIX "0x")
	SET(PREFIX "git_")
	SET(REVISION_HUMAN ${REVISION})

ELSEIF(EXISTS "${CMAKE_SOURCE_DIR}/NEWS")
	SET(REVISION_PREFIX "0x")
	SET(REVISION_HUMAN ${MAINVERSION})
	SET(PREFIX "v")
	STRING(REGEX REPLACE "([0-9]+)[.].*" "\\1" VERSION_MAJOR "${MAINVERSION}")
	IF (${VERSION_MAJOR} LESS 10)
		SET(VERSION_MAJOR "0${VERSION_MAJOR}")
	ENDIF()

	STRING(REGEX REPLACE ".*[.]0*([0-9]+)[.].*" "\\1" VERSION_MINOR "${MAINVERSION}")
	IF (${VERSION_MINOR} LESS 10)
		SET(VERSION_MINOR "0${VERSION_MINOR}")
	ENDIF()
	STRING(REGEX REPLACE ".*[.][0-9]*[.]0*([0-9]+).*" "\\1" VERSION_SUB "${MAINVERSION}")
	IF (${VERSION_SUB} LESS 10)
		SET(VERSION_SUB "0${VERSION_SUB}")
	ENDIF()
	SET(REVISION ${VERSION_MAJOR}${VERSION_MINOR}${VERSION_SUB})
	EXECUTE_PROCESS(COMMAND ${PYTHON_EXECUTABLE} -c "import os.path,time;print time.strftime('%Y-%m-%d %H:%M', time.gmtime(os.path.getmtime('${ROOT_DIR}/NEWS')))"
		OUTPUT_VARIABLE DATEINFO
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	STRING(REGEX REPLACE "([0-9]+)-.*" "\\1" VERSION_YEAR "${DATEINFO}")
	STRING(REGEX REPLACE ".*-0*([0-9]+)-.*" "\\1" VERSION_MONTH "${DATEINFO}")
	STRING(REGEX REPLACE ".*-.*-0*([0-9]+).*" "\\1" VERSION_DAY "${DATEINFO}")
	STRING(REGEX REPLACE ".* 0*([0-9]+):.*" "\\1" VERSION_HOUR "${DATEINFO}")
	STRING(REGEX REPLACE ".* .*:0*([0-9]+).*" "\\1" VERSION_MINUTE "${DATEINFO}")

ELSE()
	SET(EXTRA "UNKNOWN_VERSION")
	SET(REVISION "9999")
	SET(VERSION_YEAR "9999")
	SET(VERSION_MONTH "99")
	SET(VERSION_DAY "99")
	SET(VERSION_HOUR "99")
	SET(VERSION_MINUTE "99")
ENDIF()

SET(DATE ${VERSION_YEAR}-${VERSION_MONTH}-${VERSION_DAY})
SET(TIME ${VERSION_HOUR}:${VERSION_MINUTE})

#CONFIGURE_FILE(${SRC} ${DST} @ONLY)