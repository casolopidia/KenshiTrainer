Set-ItemProperty "HKLM:\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\10.0\Setup\VC" -Name ProductDir -Value "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\"
Set-ItemProperty "HKLM:\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\10.0\Setup\VS" -Name ProductDir -Value "C:\Program Files (x86)\Microsoft Visual Studio 10.0\"
New-Item "HKLM:\SOFTWARE\Microsoft\VisualStudio\10.0\Setup\VC" -Force | Out-Null
New-Item "HKLM:\SOFTWARE\Microsoft\VisualStudio\10.0\Setup\VS" -Force | Out-Null
Set-ItemProperty "HKLM:\SOFTWARE\Microsoft\VisualStudio\10.0\Setup\VC" -Name ProductDir -Value "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\"
Set-ItemProperty "HKLM:\SOFTWARE\Microsoft\VisualStudio\10.0\Setup\VS" -Name ProductDir -Value "C:\Program Files (x86)\Microsoft Visual Studio 10.0\"
New-Item "HKLM:\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v7.0A" -Force | Out-Null
New-Item "HKLM:\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\v7.0A" -Force | Out-Null
Set-ItemProperty "HKLM:\SOFTWARE\Microsoft\Microsoft SDKs\Windows\v7.0A" -Name InstallationFolder -Value "C:\Program Files\Microsoft SDKs\Windows\v7.1\"
Set-ItemProperty "HKLM:\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\v7.0A" -Name InstallationFolder -Value "C:\Program Files\Microsoft SDKs\Windows\v7.1\"
"DONE"
