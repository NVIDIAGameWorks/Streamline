#!/usr/bin/env pwsh

# Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

<#
.SYNOPSIS
 Script to take in a file, and convert it to a C header char-array
 representation of its binary contents. Output is written to stdout.
.EXAMPLE
 bin2cheader.ps1 -i myfile.spv
#>

param(
    [Alias("i")]
    [Parameter(Mandatory=$true)]
    [string]
    $inputFilename
)

$variable_name = $inputFilename.replace(".", "_")

# Load file into memory
# WAR for `-Encoding Byte` removed in pwsh6 (https://github.com/PowerShell/PowerShell/issues/7986)
if ($PSVersionTable.PSVersion.Major -ge 6) {
    $filevar = Get-Content $inputFilename -AsByteStream
} else {
    $filevar = Get-Content $inputFilename -Encoding Byte
}

# Print file preamble
Write-Host "unsigned char $variable_name[] = {"

$filevar | Foreach-Object {
    Write-Host -NoNewline "$_, "
}

Write-Host "};"

Write-Host "unsigned int ${variable_name}_len = $($filevar.Length);"
