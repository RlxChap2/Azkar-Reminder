$zipPath = 'E:\project\Dhikr Reminder\AzkarReminder_v1.2_Portable.zip'
$staging = 'E:\project\Dhikr Reminder\Azkar Reminder\build_rel\portable_staging'
if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
New-Item -ItemType Directory -Path $staging | Out-Null
Copy-Item 'E:\project\Dhikr Reminder\Azkar Reminder\build_rel\bin\Release\AzkarReminder.exe' -Destination $staging
Copy-Item 'E:\project\Dhikr Reminder\Azkar Reminder\build_rel\bin\Release\audio' -Destination $staging -Recurse
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path "$staging\*" -DestinationPath $zipPath -CompressionLevel Optimal
Remove-Item -Recurse -Force $staging
Get-Item $zipPath | Select-Object Name, Length, LastWriteTime
