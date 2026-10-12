Add-Type -AssemblyName System.Drawing
Get-ChildItem 'assets\images\*.png' | ForEach-Object {
    $img = [System.Drawing.Image]::FromFile($_.FullName)
    [PSCustomObject]@{
        Name = $_.Name
        Width = $img.Width
        Height = $img.Height
        Bytes = $_.Length
    }
    $img.Dispose()
} | Format-Table -AutoSize
