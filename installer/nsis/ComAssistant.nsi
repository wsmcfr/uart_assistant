; NSIS 安装脚本 - ComAssistant 串口调试助手
; 作者: ComAssistant Team
; 日期: 2026-01-16

;--------------------------------
; 包含文件
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"

;--------------------------------
; 常量定义
!define PRODUCT_NAME "ComAssistant"
!define PRODUCT_NAME_CN "串口调试助手"
!define PRODUCT_VERSION "1.0.0"
!define PRODUCT_PUBLISHER "ComAssistant Team"
!define PRODUCT_WEB_SITE "https://github.com/comassistant/comassistant"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\ComAssistant.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

;--------------------------------
; 安装程序属性
Name "${PRODUCT_NAME_CN} ${PRODUCT_VERSION}"
OutFile "ComAssistant_${PRODUCT_VERSION}_Setup.exe"
InstallDir "$PROGRAMFILES\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUnInstDetails show

;--------------------------------
; MUI 设置
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; 欢迎页面
!insertmacro MUI_PAGE_WELCOME

; 许可协议页面
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"

; 安装目录页面
!insertmacro MUI_PAGE_DIRECTORY

; 安装文件页面
!insertmacro MUI_PAGE_INSTFILES

; 完成页面
!define MUI_FINISHPAGE_RUN "$INSTDIR\ComAssistant.exe"
!define MUI_FINISHPAGE_RUN_TEXT "启动 ${PRODUCT_NAME_CN}"
!insertmacro MUI_PAGE_FINISH

; 卸载页面
!insertmacro MUI_UNPAGE_INSTFILES

; 语言设置
!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"

;--------------------------------
; 安装区段
Section "主程序" SEC01
    SetOutPath "$INSTDIR"
    SetOverwrite on

    ; 复制主程序文件
    File /r "..\..\build\Release\*.*"

    ; 创建开始菜单快捷方式
    CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME_CN}"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME_CN}\${PRODUCT_NAME_CN}.lnk" "$INSTDIR\ComAssistant.exe"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME_CN}\卸载 ${PRODUCT_NAME_CN}.lnk" "$INSTDIR\uninst.exe"

    ; 创建桌面快捷方式
    CreateShortCut "$DESKTOP\${PRODUCT_NAME_CN}.lnk" "$INSTDIR\ComAssistant.exe"
SectionEnd

Section -AdditionalIcons
    WriteIniStr "$INSTDIR\${PRODUCT_NAME}.url" "InternetShortcut" "URL" "${PRODUCT_WEB_SITE}"
    CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME_CN}\官方网站.lnk" "$INSTDIR\${PRODUCT_NAME}.url"
SectionEnd

Section -Post
    ; 写入卸载程序
    WriteUninstaller "$INSTDIR\uninst.exe"

    ; 写入注册表信息
    WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\ComAssistant.exe"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\ComAssistant.exe"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
    WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"

    ; 计算安装大小
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

;--------------------------------
; 卸载区段
Section Uninstall
    ; 删除注册表项
    DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
    DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"

    ; 删除快捷方式
    Delete "$SMPROGRAMS\${PRODUCT_NAME_CN}\${PRODUCT_NAME_CN}.lnk"
    Delete "$SMPROGRAMS\${PRODUCT_NAME_CN}\卸载 ${PRODUCT_NAME_CN}.lnk"
    Delete "$SMPROGRAMS\${PRODUCT_NAME_CN}\官方网站.lnk"
    RMDir "$SMPROGRAMS\${PRODUCT_NAME_CN}"
    Delete "$DESKTOP\${PRODUCT_NAME_CN}.lnk"

    ; 删除安装目录中的所有文件
    RMDir /r "$INSTDIR"

    SetAutoClose true
SectionEnd

;--------------------------------
; 版本信息
VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey /LANG=${LANG_SIMPCHINESE} "ProductName" "${PRODUCT_NAME_CN}"
VIAddVersionKey /LANG=${LANG_SIMPCHINESE} "Comments" "专业的串口调试工具"
VIAddVersionKey /LANG=${LANG_SIMPCHINESE} "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey /LANG=${LANG_SIMPCHINESE} "LegalCopyright" "Copyright (C) 2026 ${PRODUCT_PUBLISHER}"
VIAddVersionKey /LANG=${LANG_SIMPCHINESE} "FileDescription" "${PRODUCT_NAME_CN} 安装程序"
VIAddVersionKey /LANG=${LANG_SIMPCHINESE} "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey /LANG=${LANG_SIMPCHINESE} "ProductVersion" "${PRODUCT_VERSION}"

;--------------------------------
; 安装程序初始化
Function .onInit
    ; 检查是否已安装
    ReadRegStr $R0 ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString"
    StrCmp $R0 "" done

    MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
        "检测到 ${PRODUCT_NAME_CN} 已安装。$\n$\n点击「确定」将卸载旧版本并继续安装，或点击「取消」取消此次安装。" \
        IDOK uninst
    Abort

uninst:
    ClearErrors
    ExecWait '$R0 /S'

done:
FunctionEnd

;--------------------------------
; 卸载程序初始化
Function un.onInit
    MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 \
        "确定要完全删除 ${PRODUCT_NAME_CN} 及其所有组件吗？" \
        IDYES +2
    Abort
FunctionEnd

Function un.onUninstSuccess
    HideWindow
    MessageBox MB_ICONINFORMATION|MB_OK "${PRODUCT_NAME_CN} 已成功卸载。"
FunctionEnd
