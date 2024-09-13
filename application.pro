QT += widgets core
requires(qtConfig(filedialog))

HEADERS       =
SOURCES       = main.cpp
#! [0]
#RESOURCES     = application.qrc
#! [0]

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/mainwindows/application
INSTALLS += target
