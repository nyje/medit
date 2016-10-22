set(SOURCES
    src/AutoComplete.cxx
    src/CallTip.cxx
    src/CaseConvert.cxx
    src/CaseFolder.cxx
    src/Catalogue.cxx
    src/CellBuffer.cxx
    src/CharClassify.cxx
    src/ContractionState.cxx
    src/Decoration.cxx
    src/Document.cxx
    src/EditModel.cxx
    src/Editor.cxx
    src/EditView.cxx
    src/ExternalLexer.cxx
    src/Indicator.cxx
    src/KeyMap.cxx
    src/LineMarker.cxx
    src/MarginView.cxx
    src/PerLine.cxx
    src/PositionCache.cxx
    src/RESearch.cxx
    src/RunStyles.cxx
    src/ScintillaBase.cxx
    src/Selection.cxx
    src/Style.cxx
    src/UniConversion.cxx
    src/ViewStyle.cxx
    src/XPM.cxx

    lexlib/Accessor.cxx
    lexlib/CharacterCategory.cxx
    lexlib/CharacterSet.cxx
    lexlib/LexerBase.cxx
    lexlib/LexerModule.cxx
    lexlib/LexerSimple.cxx
    lexlib/PropSetSimple.cxx
    lexlib/StyleContext.cxx
    lexlib/WordList.cxx

    gtk/PlatGTK.cxx
    gtk/ScintillaGTK.cxx
    gtk/scintilla-marshal.c

    # win32/ScintillaWin.cxx

    lexers/LexA68k.cxx
    lexers/LexAbaqus.cxx
    lexers/LexAda.cxx
    lexers/LexAPDL.cxx
    lexers/LexAsm.cxx
    lexers/LexAsn1.cxx
    lexers/LexASY.cxx
    lexers/LexAU3.cxx
    lexers/LexAVE.cxx
    lexers/LexAVS.cxx
    lexers/LexBaan.cxx
    lexers/LexBash.cxx
    lexers/LexBasic.cxx
    lexers/LexBatch.cxx
    lexers/LexBibTeX.cxx
    lexers/LexBullant.cxx
    lexers/LexCaml.cxx
    lexers/LexCLW.cxx
    lexers/LexCmake.cxx
    lexers/LexCOBOL.cxx
    lexers/LexCoffeeScript.cxx
    lexers/LexConf.cxx
    lexers/LexCPP.cxx
    lexers/LexCrontab.cxx
    lexers/LexCsound.cxx
    lexers/LexCSS.cxx
    lexers/LexD.cxx
    lexers/LexDiff.cxx
    lexers/LexDMAP.cxx
    lexers/LexDMIS.cxx
    lexers/LexECL.cxx
    lexers/LexEiffel.cxx
    lexers/LexErlang.cxx
    lexers/LexErrorList.cxx
    lexers/LexEScript.cxx
    lexers/LexFlagship.cxx
    lexers/LexForth.cxx
    lexers/LexFortran.cxx
    lexers/LexGAP.cxx
    lexers/LexGui4Cli.cxx
    lexers/LexHaskell.cxx
    lexers/LexHex.cxx
    lexers/LexHTML.cxx
    lexers/LexInno.cxx
    lexers/LexKix.cxx
    lexers/LexKVIrc.cxx
    lexers/LexLaTeX.cxx
    lexers/LexLisp.cxx
    lexers/LexLout.cxx
    lexers/LexLua.cxx
    lexers/LexMagik.cxx
    lexers/LexMake.cxx
    lexers/LexMarkdown.cxx
    lexers/LexMatlab.cxx
    lexers/LexMetapost.cxx
    lexers/LexMMIXAL.cxx
    lexers/LexModula.cxx
    lexers/LexMPT.cxx
    lexers/LexMSSQL.cxx
    lexers/LexMySQL.cxx
    lexers/LexNimrod.cxx
    lexers/LexNsis.cxx
    lexers/LexNull.cxx
    lexers/LexOpal.cxx
    lexers/LexOScript.cxx
    lexers/LexPascal.cxx
    lexers/LexPB.cxx
    lexers/LexPerl.cxx
    lexers/LexPLM.cxx
    lexers/LexPO.cxx
    lexers/LexPOV.cxx
    lexers/LexPowerPro.cxx
    lexers/LexPowerShell.cxx
    lexers/LexProgress.cxx
    lexers/LexProps.cxx
    lexers/LexPS.cxx
    lexers/LexPython.cxx
    lexers/LexR.cxx
    lexers/LexRebol.cxx
    lexers/LexRegistry.cxx
    lexers/LexRuby.cxx
    lexers/LexRust.cxx
    lexers/LexScriptol.cxx
    lexers/LexSmalltalk.cxx
    lexers/LexSML.cxx
    lexers/LexSorcus.cxx
    lexers/LexSpecman.cxx
    lexers/LexSpice.cxx
    lexers/LexSQL.cxx
    lexers/LexSTTXT.cxx
    lexers/LexTACL.cxx
    lexers/LexTADS3.cxx
    lexers/LexTAL.cxx
    lexers/LexTCL.cxx
    lexers/LexTCMD.cxx
    lexers/LexTeX.cxx
    lexers/LexTxt2tags.cxx
    lexers/LexVB.cxx
    lexers/LexVerilog.cxx
    lexers/LexVHDL.cxx
    lexers/LexVisualProlog.cxx
    lexers/LexYAML.cxx
)

if(WIN32)
    list(APPEND SOURCES
        win32/HanjaDic.cxx
        win32/ScintRes.rc
        win32/ScintillaGtk.def
    )

add_definitions(-DWIN32)
endif(WIN32)

link_directories(${GTK_LIBRARY_DIRS})

add_library(SciLexer SHARED ${SOURCES})

target_link_libraries(SciLexer
    imm32.lib
    ole32.lib
    uuid.lib
    oleaut32.lib
    ${GTK_LIBRARIES}
)

install(TARGETS SciLexer RUNTIME DESTINATION bin)

#add_definitions(-DSCI_NAMESPACE)
add_definitions(-DGTK -DSCI_LEXER -UCXX11_REGEX)
add_definitions(-D_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES=1 -D_CRT_SECURE_NO_DEPRECATE=1)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/lexlib)

# LEXCOMPONENT=$(DIR_BIN)\SciLexer.dll

#CXXFLAGS=-Zi -TP -MP -W4 -EHsc -Zc:forScope -Zc:wchar_t $(CRTFLAGS)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4702")
list(APPEND COMPILE_DEFINITIONS_DEBUG -DDEBUG)
list(APPEND COMPILE_DEFINITIONS_RELEASE -DNDEBUG)

# !IF [cl -c -nologo CheckD2D.cxx >NUL:]
# CXXFLAGS=$(CXXFLAGS) -DDISABLE_D2D
# !MESSAGE Direct2D is not available
# !ENDIF
