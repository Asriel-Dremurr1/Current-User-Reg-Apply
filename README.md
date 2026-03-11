# Current-User-Reg-Apply
imports reg files with the parameter relative to HKCU for the current user instead of the administrator

A C++ utility for Windows designed to automate the application of registry policies to a specific user's hive by mapping `HKEY_CURRENT_USER` to their unique Security Identifier (SID).

---

## Overview

This tool identifies the SID of the current interactive user by locating the `explorer.exe` process. It then processes a standard `main.reg` file, dynamically replacing all instances of `HKEY_CURRENT_USER` with the appropriate `HKEY_USERS\<SID>` path. This is particularly useful for applying user-specific settings from an administrative or system context.

---

## Usage

1.  Place your registry settings in a file named `main.reg` in the same folder as this executable.
2.  Run the application.
3.  The tool will detect the current interactive user, rewrite the registry paths, and import them.

To run the script without a confirmation dialog or success message, use the silent flag `/s`

---

## Features

* **Automatic SID Detection**: Locates the active user's SID without manual input.
* **Dynamic Path Remapping**: Converts generic `HKCU` registry keys to absolute `HKEY_USERS` paths.
* **Unicode Support**: Handles both UTF-8 and UTF-16 (BOM) registry file formats.
* **Minimal Footprint**: Written in Win32 API with no external dependencies or CRT bloat.
* **Safety Prompt**: Displays a confirmation dialog showing the target username and SID before execution.

---

## How It Works

1.  **Process Scanning**: Uses `CreateToolhelp32Snapshot` to find `explorer.exe`.
2.  **Token Extraction**: Opens the process token to retrieve the User SID and Account Name.
3.  **File Processing**: Reads `main.reg` from the local directory.
4.  **String Substitution**: Replaces `HKEY_CURRENT_USER` strings with the user's registry hive path.
5.  **Deployment**: Creates a temporary modified registry file and imports it via `reg.exe`.
6.  **Cleanup**: Automatically deletes the temporary file after the import process completes.

---

## Requirements

* **Operating System**: Windows 7 or newer.
* **Input File**: A file named `main.reg` must be present in the same directory as the executable.
* **Permissions**: May require administrative privileges depending on the registry keys being modified.

---
