# Reads Address Library files (versionlib-*.bin) that are already on this PC and prints the offsets
# for the IDs in sheets/game_addresses.json. Read-only: it changes nothing and copies no files.
# The 1.6.1179 file is checked against the offsets already in the sheet, to prove the reader is right.
#
# Usage (from the repo folder):
#   powershell -ExecutionPolicy Bypass -File tools\versionlib_dump.ps1
#   powershell -ExecutionPolicy Bypass -File tools\versionlib_dump.ps1 -PluginDir "D:\...\Data\SKSE\Plugins"
param(
    [string]$PluginDir = "C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins",
    [string[]]$Versions = @("1-6-1179-0", "1-7-104-0")
)
$ErrorActionPreference = "Stop"

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.IO;
public static class VersionLib {
    public static string Header;
    // Address Library format 2 (CommonLibSSE IDDatabase): delta-encoded (id, offset) pairs.
    public static Dictionary<ulong, ulong> Load(string path) {
        var r = new BinaryReader(File.OpenRead(path));
        try {
            int format = r.ReadInt32();
            int v0 = r.ReadInt32(), v1 = r.ReadInt32(), v2 = r.ReadInt32(), v3 = r.ReadInt32();
            int nameLen = r.ReadInt32();
            string name = System.Text.Encoding.ASCII.GetString(r.ReadBytes(nameLen));
            int ptrSize = r.ReadInt32();
            int count = r.ReadInt32();
            Header = String.Format("format={0} version={1}.{2}.{3}.{4} name={5} ptr={6} count={7}",
                format, v0, v1, v2, v3, name, ptrSize, count);
            if (format != 2) throw new Exception("unknown format " + format);
            var map = new Dictionary<ulong, ulong>(count);
            ulong prevId = 0, prevOff = 0;
            for (int i = 0; i < count; i++) {
                byte type = r.ReadByte();
                int lo = type & 0xF, hi = type >> 4;
                ulong id;
                switch (lo) {
                    case 0: id = r.ReadUInt64(); break;
                    case 1: id = prevId + 1; break;
                    case 2: id = prevId + r.ReadByte(); break;
                    case 3: id = prevId - r.ReadByte(); break;
                    case 4: id = prevId + r.ReadUInt16(); break;
                    case 5: id = prevId - r.ReadUInt16(); break;
                    case 6: id = r.ReadUInt16(); break;
                    case 7: id = r.ReadUInt32(); break;
                    default: throw new Exception("bad id type at entry " + i);
                }
                ulong tp = (hi & 8) != 0 ? prevOff / (ulong)ptrSize : prevOff;
                ulong off;
                switch (hi & 7) {
                    case 0: off = r.ReadUInt64(); break;
                    case 1: off = tp + 1; break;
                    case 2: off = tp + r.ReadByte(); break;
                    case 3: off = tp - r.ReadByte(); break;
                    case 4: off = tp + r.ReadUInt16(); break;
                    case 5: off = tp - r.ReadUInt16(); break;
                    case 6: off = r.ReadUInt16(); break;
                    default: off = r.ReadUInt32(); break;
                }
                if ((hi & 8) != 0) off *= (ulong)ptrSize;
                map[id] = off;
                prevId = id; prevOff = off;
            }
            return map;
        } finally { r.Close(); }
    }
}
'@

$sheet = Get-Content (Join-Path $PSScriptRoot "..\sheets\game_addresses.json") -Raw | ConvertFrom-Json
foreach ($v in $Versions) {
    $path = Join-Path $PluginDir "versionlib-$v.bin"
    $dotted = $v -replace "-", "."
    "=== $dotted ==="
    if (-not (Test-Path $path)) { "missing: $path"; continue }
    try {
        $map = [VersionLib]::Load($path)
    } catch {
        "error: $($_.Exception.Message)"
        "header: $([VersionLib]::Header)"
        "first 64 bytes: " + (([IO.File]::ReadAllBytes($path)[0..63] | ForEach-Object { $_.ToString("x2") }) -join " ")
        continue
    }
    "header: $([VersionLib]::Header)"
    foreach ($row in $sheet.rows) {
        $id = [uint64]$row.ae_id
        $got = if ($map.ContainsKey($id)) { "0x{0:x}" -f $map[$id] } else { "" }
        $expected = $row.offsets.$dotted
        $check = if ($expected) { if ($expected -eq $got) { "matches sheet" } else { "MISMATCH, sheet has $expected" } } else { "" }
        "{0,-22} {1,-7} {2,-10} {3}" -f $row.id, $row.ae_id, $(if ($got) { $got } else { "not found" }), $check
    }
}
