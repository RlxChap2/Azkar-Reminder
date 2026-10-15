$certSubject = "CN=Ahmed Mohamed Kamel"
$cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq $certSubject } | Select-Object -First 1

if (!$cert) {
    Write-Host "Creating self-signed code signing certificate..."
    $cert = New-SelfSignedCertificate `
        -Type Custom `
        -Subject $certSubject `
        -DnsName "AzkarReminder" `
        -KeyUsage DigitalSignature `
        -FriendlyName "Azkar Reminder Developer Certificate" `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3")
}

Write-Host "Certificate Thumbprint: $($cert.Thumbprint)"

$cerPath = "release\AzkarReminder-DeveloperCert.cer"
Export-Certificate -Cert $cert -FilePath $cerPath -Force | Out-Null
Write-Host "Exported CER to: $cerPath"

$signtool = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe"
& $signtool sign /fd SHA256 /a /sha1 $cert.Thumbprint "release\AzkarReminder-1.2.4.msix"
Write-Host "MSIX signed successfully!"
