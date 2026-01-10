function Find-SerialPort {
    Get-PnpDevice -Class Ports -PresentOnly | Select-Object Name, DeviceID, Status, @{Name='HardwareID';Expression={$_.HardwareID -join ', '}} | ForEach-Object {
        $name = $_.Name
        $deviceID = $_.DeviceID
        $hardwareIDs = $_.HardwareID

        # Extract VID and PID from HardwareID
        $vid = ($hardwareIDs | Where-Object {$_ -like '*VID_*'} | ForEach-Object {
            if ($_ -match 'VID_([0-9A-F]{4})') { $Matches[1] }
        }) -join ', '

        $productId = ($hardwareIDs | Where-Object {$_ -like '*PID_*'} | ForEach-Object {
            if ($_ -match 'PID_([0-9A-F]{4})') { $Matches[1] }
        }) -join ', '

        # ESP32-C3 (VID=303A, PID=1001)
        if ($vid.ToUpper() -eq "303A" -and $productId.ToUpper() -eq "1001") {
            if ($name -match 'COM\d+') {
                return $Matches[0]
            }
        }
        
#         # ESP32 (VID=10C4, PID=EA60) - Silicon Labs CP210x
#         if ($vid.ToUpper() -eq "10C4" -and $productId.ToUpper() -eq "EA60") {
#         if ($name -match 'COM\d+') {
#         return $Matches[0]
#         }
# }
        
#         # ESP32 (VID=1A86, PID=7523) - QinHeng Electronics CH340
#         if ($vid.ToUpper() -eq "1A86" -and $productId.ToUpper() -eq "7523") {
#         if ($name -match 'COM\d+') {
#         return $Matches[0]
#         }
# }
    }
    return $null
}

Export-ModuleMember -Function Find-SerialPort