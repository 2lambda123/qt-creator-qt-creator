add_qtc_plugin(CompilationDatabaseProjectManager
  DEPENDS Utils
  PLUGIN_DEPENDS Core CppEditor ProjectExplorer TextEditor
  SOURCES
    compilationdatabaseconstants.h
    compilationdatabaseproject.cpp
    compilationdatabaseproject.h
    compilationdatabaseprojectmanagerplugin.cpp
    compilationdatabaseprojectmanagertr.h
    compilationdatabaseutils.cpp
    compilationdatabaseutils.h
    compilationdbparser.cpp
    compilationdbparser.h
)

extend_qtc_plugin(CompilationDatabaseProjectManager
  CONDITION WITH_TESTS
  SOURCES
    compilationdatabasetests.cpp
    compilationdatabasetests.h
    compilationdatabasetests.qrc
)
