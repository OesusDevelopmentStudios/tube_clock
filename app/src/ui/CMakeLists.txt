# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Widgets SerialPort DBus)
# Individual packages: the KF6 umbrella config needs extra-cmake-modules.
foreach(fw Kirigami I18n CoreAddons DNSSD)
  find_package(KF6${fw} 6.8 REQUIRED)
endforeach()
find_package(Qt6Keychain REQUIRED)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

qt_add_executable(tubeclock main.cpp)

qt_add_qml_module(tubeclock
  URI TubeClock
  VERSION 1.0
  # Qt and KDE are shared libraries here; the static plugin import scan only warns.
  NO_IMPORT_SCAN
  SOURCES
    AppSettings.cpp AppSettings.hpp
    ClockListModel.cpp ClockListModel.hpp
    ConfigForm.cpp ConfigForm.hpp
    DeviceController.cpp DeviceController.hpp
    FlashController.cpp FlashController.hpp
    HostResolver.cpp HostResolver.hpp
    ImageSetModel.cpp ImageSetModel.hpp
    SerialPorts.cpp SerialPorts.hpp
    TokenStore.cpp TokenStore.hpp
  QML_FILES
    qml/Main.qml
    qml/DisclaimerPage.qml
    qml/ClocksPage.qml
    qml/DevicePage.qml
    qml/SetupConnectPage.qml
    qml/SetupNetworkPage.qml
    qml/SetupTimePage.qml
    qml/SetupImagesPage.qml
    qml/SetupFlashPage.qml
    qml/BackupsPage.qml
    qml/DamageWarning.qml
    qml/TimeZoneField.qml
    qml/ImportSetDialog.qml
    qml/TubePreview.qml
    qml/BacklightFields.qml
)

target_compile_definitions(tubeclock PRIVATE
  TUBECLOCK_APP_ID="${APP_ID}"
  TUBECLOCK_VERSION="${PROJECT_VERSION}"
  TRANSLATION_DOMAIN="tubeclock")
target_link_libraries(tubeclock PRIVATE
  tubeclock_core
  Qt6::Quick Qt6::QuickControls2 Qt6::Widgets Qt6::SerialPort Qt6::DBus
  KF6::I18n KF6::I18nQml KF6::CoreAddons KF6::DNSSD
  Qt6Keychain::Qt6Keychain)

install(TARGETS tubeclock DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${REPO_ROOT}/packaging/${APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES ${REPO_ROOT}/packaging/${APP_ID}.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES ${REPO_ROOT}/packaging/${APP_ID}.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
