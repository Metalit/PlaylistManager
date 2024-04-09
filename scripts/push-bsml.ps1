Param(
    [Parameter(Mandatory=$false)]
    [Switch] $clear,

    [Parameter(Mandatory=$false)]
    [String] $location = "assets",

    [Parameter(Mandatory=$false)]
    [String] $destination = "/sdcard"
)

if ($clear -eq $true) {
    & adb shell rm $destination/*.bsml
    Write-Output "Removed BSML files in $destination"
}
else {
    $files = Get-ChildItem -Path $location/*.bsml -Name

    foreach ($file in $files) {
        & adb push $location/$file $destination/$file
    }
}
