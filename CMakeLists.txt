cmake_minimum_required(VERSION 3.16)
project(cd-debugtool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets SerialPort Network Test)

set(PROJECT_SOURCES
    src/main.cpp

    src/config/ConfigManager.cpp
    src/config/ConfigManager.h

    src/core/ThemeManager.cpp
    src/core/ThemeManager.h
    src/core/ProtocolRegistry.cpp
    src/core/ProtocolRegistry.h
    src/core/IProtocolPage.h
    src/core/Device.h
    src/core/DeviceManager.cpp
    src/core/DeviceManager.h
    src/core/UpgradeEngine.cpp
    src/core/UpgradeEngine.h

    src/protocol/rs485/Rs485Frame.cpp
    src/protocol/rs485/Rs485Frame.h
    src/protocol/rs485/Rs485Commands.cpp
    src/protocol/rs485/Rs485Commands.h

    src/protocol/iap/Crc32Mpeg2.cpp
    src/protocol/iap/Crc32Mpeg2.h
    src/protocol/iap/IapFrame.cpp
    src/protocol/iap/IapFrame.h
    src/protocol/iap/IapCommands.cpp
    src/protocol/iap/IapCommands.h

    src/protocol/qinghai/QingHaiProtocol.cpp
    src/protocol/qinghai/QingHaiProtocol.h

    src/protocol/sichuan_etc/SiChuanEtcProtocol.cpp
    src/protocol/sichuan_etc/SiChuanEtcProtocol.h

    src/protocol/sichuan_mtc/SiChuanMtcProtocol.cpp
    src/protocol/sichuan_mtc/SiChuanMtcProtocol.h

    src/protocol/sichuan_ol/SiChuanOlProtocol.cpp
    src/protocol/sichuan_ol/SiChuanOlProtocol.h

    src/protocol/shandong/ShanDongProtocol.cpp
    src/protocol/shandong/ShanDongProtocol.h

    src/protocol/common/Codec.h

    src/transport/SerialTransport.cpp
    src/transport/SerialTransport.h
    src/transport/UdpTransport.cpp
    src/transport/UdpTransport.h

    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/LoginDialog.cpp
    src/ui/LoginDialog.h

    src/ui/widgets/LogPanel.cpp
    src/ui/widgets/LogPanel.h
    src/ui/widgets/ConnectConfigPanel.cpp
    src/ui/widgets/ConnectConfigPanel.h
    src/ui/widgets/HexDumpPanel.cpp
    src/ui/widgets/HexDumpPanel.h
    src/ui/widgets/LedIndicator.cpp
    src/ui/widgets/LedIndicator.h
    src/ui/widgets/DisplayWidget.cpp
    src/ui/widgets/DisplayWidget.h
    src/ui/widgets/BrightnessWidget.cpp
    src/ui/widgets/BrightnessWidget.h
    src/ui/widgets/DacScaleWidget.cpp
    src/ui/widgets/DacScaleWidget.h

    src/ui/pages/QingHaiPage.cpp
    src/ui/pages/QingHaiPage.h
    src/ui/pages/SerialProtocolPage.cpp
    src/ui/pages/SerialProtocolPage.h
    src/ui/pages/SiChuanEtcPage.cpp
    src/ui/pages/SiChuanEtcPage.h
    src/ui/pages/SiChuanMtcPage.cpp
    src/ui/pages/SiChuanMtcPage.h
    src/ui/pages/SiChuanOlPage.cpp
    src/ui/pages/SiChuanOlPage.h
    src/ui/pages/ShanDongPage.cpp
    src/ui/pages/ShanDongPage.h
    src/ui/pages/IapPage.cpp
    src/ui/pages/IapPage.h
    src/ui/pages/IapDevicePanel.cpp
    src/ui/pages/IapDevicePanel.h
    src/ui/pages/IapUpgradePanel.cpp
    src/ui/pages/IapUpgradePanel.h
    src/ui/pages/Rs485Page.cpp
    src/ui/pages/Rs485Page.h
    src/ui/pages/Rs485ControlPanel.cpp
    src/ui/pages/Rs485ControlPanel.h

    resources/resources.qrc
)

add_executable(cd-debugtool ${PROJECT_SOURCES})

target_link_libraries(cd-debugtool PRIVATE Qt5::Widgets Qt5::SerialPort Qt5::Network)

target_include_directories(cd-debugtool PRIVATE src)

install(TARGETS cd-debugtool RUNTIME DESTINATION usr/bin)
install(FILES resources/icons/cd-debugtool.svg DESTINATION usr/share/icons/hicolor/scalable/apps RENAME cd-debugtool.svg)
install(FILES debian/cd-debugtool.desktop DESTINATION usr/share/applications)

enable_testing()

add_executable(cd-debugtool-tests
    tests/main.cpp
    tests/test_rs485.cpp
    tests/test_crc32.cpp
    tests/test_iap_frame.cpp
    tests/test_qinghai.cpp
    tests/test_sc_etc.cpp
    tests/test_sc_mtc.cpp
    tests/test_sc_ol.cpp
    tests/test_shandong.cpp
    tests/test_config.cpp
    tests/test_upgrade_engine.cpp

    src/config/ConfigManager.cpp
    src/config/ConfigManager.h

    src/core/UpgradeEngine.cpp
    src/core/UpgradeEngine.h
    src/core/Device.h

    src/protocol/rs485/Rs485Frame.cpp
    src/protocol/rs485/Rs485Frame.h
    src/protocol/rs485/Rs485Commands.cpp
    src/protocol/rs485/Rs485Commands.h

    src/protocol/iap/Crc32Mpeg2.cpp
    src/protocol/iap/Crc32Mpeg2.h
    src/protocol/iap/IapFrame.cpp
    src/protocol/iap/IapFrame.h
    src/protocol/iap/IapCommands.cpp
    src/protocol/iap/IapCommands.h

    src/protocol/qinghai/QingHaiProtocol.cpp
    src/protocol/qinghai/QingHaiProtocol.h

    src/protocol/sichuan_etc/SiChuanEtcProtocol.cpp
    src/protocol/sichuan_etc/SiChuanEtcProtocol.h

    src/protocol/sichuan_mtc/SiChuanMtcProtocol.cpp
    src/protocol/sichuan_mtc/SiChuanMtcProtocol.h

    src/protocol/sichuan_ol/SiChuanOlProtocol.cpp
    src/protocol/sichuan_ol/SiChuanOlProtocol.h

    src/protocol/shandong/ShanDongProtocol.cpp
    src/protocol/shandong/ShanDongProtocol.h
)

target_link_libraries(cd-debugtool-tests PRIVATE Qt5::Core Qt5::Test)

target_include_directories(cd-debugtool-tests PRIVATE src)

add_test(NAME cd-debugtool-tests COMMAND cd-debugtool-tests)
