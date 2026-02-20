# BaluDesk NSIS Installer - Setup & Build Guide

**Version**: 1.0.0  
**Date**: 2025-01-05  
**Target**: Windows x64

---

## 📋 Voraussetzungen

### ✅ Bereits vorhanden
- Node.js + npm ✓
- React Frontend ✓
- C++ Backend (baludesk-backend.exe) ✓
- TypeScript kompiliert ✓

### ❌ Muss installiert werden
- **NSIS** (Nullsoft Scriptable Install System)

---

## 1️⃣ NSIS Installation

### Schritt 1: Download

1. Gehe zu: https://nsis.sourceforge.io/Download
2. Download die neueste Version (aktuell 3.10)
3. Wähle: **NSIS-3.10-setup.exe**

### Schritt 2: Installation

1. Starte **NSIS-3.10-setup.exe**
2. Akzeptiere die Lizenz
3. Wähle Installation Path: `C:\Program Files (x86)\NSIS`
4. **WICHTIG**: Beim "Select Components" Screen:
   - ☑️ "NSIS Core"
   - ☑️ "Plugins"
   - ☑️ "Include files"
   - ☑️ "Add NSIS to System PATH" (Falls Option vorhanden)

5. Fertigstellen

### Schritt 3: Verify Installation

Öffne PowerShell und führe aus:
```powershell
makensis /version
```

**Erwartete Ausgabe:**
```
NSIS 3.10
Copyright 1999-2024 the NSIS contributors
```

Falls nicht gefunden:
```powershell
# Manuell zu PATH hinzufügen
$env:Path += ";C:\Program Files (x86)\NSIS"
# Oder: System Settings → Environment Variables → System variables → Path → Add "C:\Program Files (x86)\NSIS"
```

---

## 2️⃣ Installer bauen

### Methode A: Mit Build-Script (Empfohlen)

```batch
cd f:\Programme (x86)\BaluDesk\frontend
build-installer.bat
```

**Was passiert:**
1. ✓ Cleaned old build artifacts
2. ✓ TypeScript kompiliert (Electron Main)
3. ✓ React Frontend gebaut (Vite)
4. ✓ NSIS Installer erstellt
5. ✓ Output: `dist-electron\BaluDesk-Setup-1.0.0.exe`

### Methode B: Manuell

```batch
cd f:\Programme (x86)\BaluDesk\frontend

# 1. TypeScript kompilieren
npm run compile

# 2. React bauen
vite build

# 3. NSIS Installer erstellen
makensis.exe /V3 "BaluDesk-Installer.nsi"
```

---

## 3️⃣ Installer testen

### Installation testen

```batch
# Starte den Installer
dist-electron\BaluDesk-Setup-1.0.0.exe
```

**Installer sollte:**
1. Welcome-Seite zeigen
2. Installationsverzeichnis fragen (default: C:\Program Files\BaluDesk)
3. Dateien kopieren
4. Shortcuts erstellen:
   - Start Menu: `BaluDesk.lnk`
   - Desktop: `BaluDesk.lnk`
5. Fertig!

### Post-Installation Checks

```batch
# 1. Prüfe Installationsverzeichnis
ls "C:\Program Files\BaluDesk\"
# Sollte enthalten: dist/, node_modules/, backend/, electron.exe, etc.

# 2. Prüfe Start Menu
dir "%APPDATA%\Microsoft\Windows\Start Menu\Programs\BaluDesk\"
# Sollte enthalten: BaluDesk.lnk, Uninstall.lnk

# 3. Prüfe Registry
reg query "HKCU\Software\BaluDesk"
# Sollte zeigen: Install_Dir = C:\Program Files\BaluDesk
```

### Anwendung starten

Klicke auf:
1. Desktop Icon "BaluDesk"
2. Oder Start Menu → BaluDesk → BaluDesk
3. Oder: `C:\Program Files\BaluDesk\electron.exe`

### Deinstallation testen

Start Menu → BaluDesk → Uninstall.lnk

**Sollte:**
- Alle Dateien löschen
- Shortcuts entfernen
- Registry cleanup durchführen

---

## 4️⃣ Konfigurationen & Customization

### NSIS Script bearbeiten

Datei: `baludesk\frontend\BaluDesk-Installer.nsi`

#### Sprachen hinzufügen

```nsis
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "German"
!insertmacro MUI_LANGUAGE "French"    ; Hinzufügen für Französisch
```

#### Installation verzeichnis ändern

```nsis
InstallDir "$PROGRAMFILES\BaluDesk"  ; Aktuell
InstallDir "D:\Apps\BaluDesk"        ; Beispiel: Custom Path
```

#### System Requirements

```nsis
; Im .onInit Funktion anpassen:
Function .onInit
  ${If} ${RunningX64}
    ; 64-bit Windows
  ${Else}
    MessageBox MB_OK "BaluDesk requires 64-bit Windows"
    Abort
  ${EndIf}
  
  ; Optional: Minimale RAM-Check
  ; Optional: Windows Version Check
FunctionEnd
```

#### Icons ändern

```nsis
; Icon für Shortcuts
CreateShortcut "$SMPROGRAMS\BaluDesk\BaluDesk.lnk" \
  "$INSTDIR\electron.exe" \
  "" \
  "$INSTDIR\custom-icon.ico"  ; Eigenes Icon
```

---

## 📊 Output-Struktur

Nach erfolgreichem Build:

```
baludesk/frontend/dist-electron/
├── BaluDesk-Setup-1.0.0.exe         ← Installer für Benutzer
├── BaluDesk-Setup-1.0.0.exe.bak    ← Backup (optional)
└── builder-effective-config.yaml    ← NSIS Config (Debug)
```

**Größe des Installers:**
- Typisch: 150-200 MB (komprimiert)
- Installed: 300-400 MB (auf Disk)

---

## 🔧 Troubleshooting

### Problem: "makensis is not recognized"

**Lösung 1**: NSIS zum PATH hinzufügen
```powershell
$env:Path += ";C:\Program Files (x86)\NSIS"
echo $env:Path
```

**Lösung 2**: Absoluten Pfad verwenden
```batch
"C:\Program Files (x86)\NSIS\makensis.exe" /V3 "BaluDesk-Installer.nsi"
```

### Problem: "File not found: electron.exe"

**Ursache**: Vite build nicht erfolgreich  
**Lösung**:
```batch
npm run compile
vite build
ls dist/
# Sollte enthalten: index.html, assets/, main/
```

### Problem: "Backend .exe not found"

**Ursache**: C++ Backend nicht kompiliert  
**Lösung**:
```batch
cd baludesk\backend
cmake --build build --config Release
ls build\Release\baludesk-backend.exe
```

### Problem: Installer startet nicht

**Debugging**:
```batch
# Verbose mode
"C:\Program Files (x86)\NSIS\makensis.exe" /V4 "BaluDesk-Installer.nsi"
```

---

## 📦 Distribution

### Für Benutzer bereitstellen

**Website**:
```
Download: BaluDesk-Setup-1.0.0.exe
Size: ~180 MB
Requirements: Windows 10/11 x64
```

**GitHub Release**:
```
Assets: BaluDesk-Setup-1.0.0.exe
Instructions: Download & Run → Next → Finish
```

**USB-Stick**:
```
Kopiere: BaluDesk-Setup-1.0.0.exe
Benutzer: Plugged USB → start installer
```

---

## ✅ Checklist vor Release

- [ ] NSIS installiert und im PATH
- [ ] `npm run compile` erfolgreich
- [ ] `vite build` erfolgreich
- [ ] `makensis.exe` findet BaluDesk-Installer.nsi
- [ ] Installer erstellt: `dist-electron\BaluDesk-Setup-1.0.0.exe`
- [ ] Installer getestet auf test-machine
- [ ] Installation erfolgreich
- [ ] Anwendung startet
- [ ] Deinstallation funktioniert
- [ ] Icons & Shortcuts korrekt
- [ ] Registry-Einträge gesetzt

---

## 🚀 Quick Start

```bash
# 1. NSIS installieren (einmalig)
# Download von https://nsis.sourceforge.io

# 2. Installer bauen
cd baludesk\frontend
build-installer.bat

# 3. Test
dist-electron\BaluDesk-Setup-1.0.0.exe

# 4. Distribute!
```

---

**Status**: ✅ Ready to Build Professional Installer

