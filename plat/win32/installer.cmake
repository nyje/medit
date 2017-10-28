set(MOO_INSTALL_BDIST TRUE CACHE BOOL "Install gtk and python binaries")

if(MOO_INSTALL_BDIST)
    install(DIRECTORY ${MOO_GTK_DIST_DIR}/bin DESTINATION .)
    install(DIRECTORY ${MOO_GTK_DIST_DIR}/lib DESTINATION .)
    install(DIRECTORY ${MOO_GTK_DIST_DIR}/share DESTINATION .)
    install(DIRECTORY ${MOO_PYTHON_DIST_DIR}/bin DESTINATION .)
endif()

if(MSVC12)
    set(MOO_VC_CRT_DIR "C:/Program Files (x86)/Microsoft Visual Studio 12.0/VC/redist/x64/Microsoft.VC120.CRT")
    install(FILES "${MOO_VC_CRT_DIR}/msvcp120.dll" "${MOO_VC_CRT_DIR}/msvcr120.dll" DESTINATION bin)
else()
    message(FATAL_ERROR "Don't know where redist CRT libraries are")
endif()

#INCLUDE(InstallRequiredSystemLibraries)

SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "medit")
SET(CPACK_PACKAGE_VENDOR "Yevgen Muntyan")
#SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ReadMe.txt")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")
SET(CPACK_PACKAGE_VERSION_MAJOR ${MOO_MAJOR_VERSION})
SET(CPACK_PACKAGE_VERSION_MINOR ${MOO_MINOR_VERSION})
SET(CPACK_PACKAGE_VERSION_PATCH ${MOO_MICRO_VERSION})
SET(CPACK_PACKAGE_INSTALL_DIRECTORY "medit")
# There is a bug in NSI that does not handle full unix paths properly. Make
# sure there is at least one set of four (4) backlasshes.
#SET(CPACK_PACKAGE_ICON "${CMake_SOURCE_DIR}/Utilities/Release\\\\InstallIcon.bmp")
SET(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\medit.exe")
SET(CPACK_NSIS_DISPLAY_NAME "medit")
SET(CPACK_NSIS_HELP_LINK "http:\\\\\\\\mooedit.sourceforge.net")
SET(CPACK_NSIS_URL_INFO_ABOUT "http:\\\\\\\\mooedit.sourceforge.net")
SET(CPACK_NSIS_CONTACT "emuntyan@users.sourceforge.net")
#SET(CPACK_NSIS_MODIFY_PATH ON)
SET(CPACK_PACKAGE_EXECUTABLES "medit" "medit")

INCLUDE(CPack)