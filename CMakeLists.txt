CMAKE_MINIMUM_REQUIRED(VERSION 2.4)
SET (PROJECTNAME omwiki)
SET (VERSION "1.8")
PROJECT(${PROJECTNAME} C)

SET(CMAKE_C_FLAGS "-Os -ffunction-sections -fdata-sections -I ${CMAKE_CURRENT_BINARY_DIR}/src -I ${CMAKE_CURRENT_SOURCE_DIR}/src")
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections")
CONFIGURE_FILE(config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/src/config.h)

SET(SOURCES
  src/didi.c
  src/http.c
  src/util.c
  src/wikiashtml.c
  src/wiki.c
  src/wikichanges.c
  src/wikientries.c
  src/wikilogin.c)
SET(HEADERS
  src/didi.h
  src/favicon.h
  src/http.h
  src/util.h
  src/wikiashtml.h
  src/wikichanges.h
  src/wikientries.h
  src/wiki.h
  src/wikilogin.h
  src/wikitext.h)
SET(DOCS
  INSTALL
  NEWS
  TODO
  COPYING
  README)

ADD_EXECUTABLE(${PROJECTNAME} ${SOURCES} ${HEADERS})
INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${PROJECTNAME} DESTINATION bin)
