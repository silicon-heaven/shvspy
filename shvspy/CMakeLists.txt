qt_add_executable(shvspy
    src/accessmodel/accessitemdelegate.cpp
    src/accessmodel/accessmodel.cpp
    src/attributesmodel/attributesmodel.cpp
    src/dlgaddeditmount.cpp
    src/dlgaddeditrole.cpp
    src/dlgaddedituser.cpp
    src/dlgcallshvmethod.cpp
    src/dlgmountseditor.cpp
    src/dlgroleseditor.cpp
    src/dlgselectroles.cpp
    src/dlgsubscriptionparameters.cpp
    src/dlguserseditor.cpp
    src/log/rpcnotificationsmodel.cpp
    src/methodparametersdialog.cpp
    src/rolestreemodel/rolestreemodel.cpp
    src/servertreemodel/servertreemodel.cpp
    src/servertreemodel/shvnodeitem.cpp
    src/servertreemodel/shvbrokernodeitem.cpp
    src/servertreeview.cpp
    src/subscriptionsmodel/subscriptionsmodel.cpp
    src/subscriptionsmodel/subscriptionstableitemdelegate.cpp
    src/subscriptionswidget.cpp
    src/texteditdialog.cpp
    src/theapp.cpp
    src/appclioptions.cpp
    src/main.cpp
    src/mainwindow.cpp
	src/dlgbrokerproperties.cpp
	src/brokerproperty.cpp
    shvspy.qrc
    config/config.qrc
)

if(USE_QT6 AND NOT EMSCRIPTEN)
    target_link_libraries(shvspy PRIVATE libshv::libshvvisu Qt::NetworkAuth)
    target_compile_definitions(shvspy PRIVATE "WITH_AZURE_SUPPORT")
else()
    target_link_libraries(shvspy PRIVATE libshv::libshvvisu)
endif()

target_include_directories(shvspy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS shvspy)
