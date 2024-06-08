# Constants
$VERSION = "0.3"
$BLUE = "`e[94m"
$GREEN = "`e[92m"
$YELLOW = "`e[93m"
$RED = "`e[91m"
$ENDC = "`e[0m"

function Display-Banner {
    $banner = @"
___________     __         .__ __________                     .__  .__   
\__    ___/____/  |______  |  |\______   \ ____   ____ _____  |  | |  |  
  |    | /  _ \   __\__  \ |  | |       _// __ \_/ ___\\__  \ |  | |  |  
  |    |(  <_> )  |  / __ \|  |_|    |   \  ___/\  \___ / __ \|  |_|  |__
  |____| \____/|__| (____  /____/____|_  /\___  >\___  >____  /____/____/
                         \/            \/     \/     \/     \/           
v$VERSION / Alexander Hagenah / @xaitax / ah@primepage.de / martian58
"@
    Write-Host -NoNewline "$BLUE$banner$ENDC"
}

function Modify-Permissions {
    param (
        [string]$path
    )
    try {
        $null = icacls $path /grant "$env:USERNAME:(OI)(CI)F" /T /C /Q
        Write-Host "$GREEN`u2714 Permissions modified for $path and all its subdirectories and files$ENDC"
    }
    catch {
        Write-Host "$RED`u274c Failed to modify permissions for $path: $_$ENDC"
    }
}

function Main {
    param (
        [string]$from_date = $null,
        [string]$to_date = $null,
        [string]$search_term = $null
    )

    Display-Banner
    $username = $env:USERNAME
    $base_path = "C:\Users\$username\AppData\Local\CoreAIPlatform.00\UKP"

    if (-not (Test-Path -Path $base_path)) {
        Write-Host "🚫 Base path does not exist."
        return
    }

    Modify-Permissions -path $base_path
    $guid_folder = Get-ChildItem -Path $base_path -Directory | Select-Object -First 1

    if (-not $guid_folder) {
        Write-Host "🚫 Could not find the GUID folder."
        return
    }

    $guid_folder_path = $guid_folder.FullName
    Write-Host "📁 Recall folder found: $guid_folder_path"

    $db_path = Join-Path -Path $guid_folder_path -ChildPath "ukg.db"
    $image_store_path = Join-Path -Path $guid_folder_path -ChildPath "ImageStore"

    if (-not (Test-Path -Path $db_path) -or -not (Test-Path -Path $image_store_path)) {
        Write-Host "🚫 Windows Recall feature not found. Nothing to extract."
        return
    }

    $proceed = Read-Host "🟢 Windows Recall feature found. Do you want to proceed with the extraction? (yes/no)"
    if ($proceed -ne "yes") {
        Write-Host "⚠️ Extraction aborted."
        return
    }

    $timestamp = Get-Date -Format "yyyy-MM-dd-HH-mm"
    $extraction_folder = Join-Path -Path (Get-Location) -ChildPath "${timestamp}_Recall_Extraction"
    New-Item -Path $extraction_folder -ItemType Directory -Force | Out-Null
    Write-Host "📂 Creating extraction folder: $extraction_folder`n"

    Copy-Item -Path $db_path -Destination $extraction_folder
    Copy-Item -Path $image_store_path -Destination $extraction_folder -Recurse -Force

    Get-ChildItem -Path (Join-Path -Path $extraction_folder -ChildPath "ImageStore") | ForEach-Object {
        $image_path = $_.FullName
        if ($image_path -notlike "*.jpg") {
            Rename-Item -Path $image_path -NewName "$image_path.jpg"
        }
    }

    $db_extraction_path = Join-Path -Path $extraction_folder -ChildPath "ukg.db"
    $conn = [System.Data.SQLite.SQLiteConnection]::new("Data Source=$db_extraction_path;Version=3;")
    $conn.Open()

    $from_date_timestamp = if ($from_date) { [int]([datetime]::Parse($from_date).AddHours(-5).ToUnixTimeMilliseconds()) } else { $null }
    $to_date_timestamp = if ($to_date) { [int]([datetime]::Parse($to_date).AddDays(1).AddHours(-5).ToUnixTimeMilliseconds()) } else { $null }

    $query = "SELECT WindowTitle, TimeStamp, ImageToken FROM WindowCapture WHERE (WindowTitle IS NOT NULL OR ImageToken IS NOT NULL)"
    $cmd = $conn.CreateCommand()
    $cmd.CommandText = $query
    $reader = $cmd.ExecuteReader()

    $captured_windows = @()
    $images_taken = @()

    while ($reader.Read()) {
        $window_title = $reader["WindowTitle"]
        $timestamp = [int]$reader["TimeStamp"]
        $image_token = $reader["ImageToken"]

        if (($from_date_timestamp -eq $null -or $from_date_timestamp -le $timestamp) -and ($to_date_timestamp -eq $null -or $timestamp -lt $to_date_timestamp)) {
            $readable_timestamp = ([datetime]::FromFileTimeUtc($timestamp * 10000)).ToString("yyyy-MM-dd HH:mm:ss")
            if ($window_title) {
                $captured_windows += "[$readable_timestamp] $window_title"
            }
            if ($image_token) {
                $images_taken += "[$readable_timestamp] $image_token"
            }
        }
    }

    $reader.Close()
    $cmd.Dispose()
    $conn.Close()

    $captured_windows_count = $captured_windows.Count
    $images_taken_count = $images_taken.Count
    $output = @(
        "🪟 Captured Windows: $captured_windows_count",
        "📸 Images Taken: $images_taken_count"
    )

    if ($search_term) {
        $conn.Open()
        $search_query = "SELECT c1, c2 FROM WindowCaptureTextIndex_content WHERE c1 LIKE '%$search_term%' OR c2 LIKE '%$search_term%'"
        $cmd = $conn.CreateCommand()
        $cmd.CommandText = $search_query
        $reader = $cmd.ExecuteReader()

        $search_results = @()
        while ($reader.Read()) {
            $search_results += "c1: $($reader["c1"]), c2: $($reader["c2"])"
        }

        $reader.Close()
        $cmd.Dispose()
        $conn.Close()

        $search_results_count = $search_results.Count
        $output += "🔍 Search results for '$search_term': $search_results_count"
    }

    $output_file_path = Join-Path -Path $extraction_folder -ChildPath "TotalRecall.txt"
    Set-Content -Path $output_file_path -Value "Captured Windows:`n$($captured_windows -join "`n")`n`nImages Taken:`n$($images_taken -join "`n")"
    if ($search_term) {
        Add-Content -Path $output_file_path -Value "`n`nSearch Results:`n$($search_results -join "`n")"
    }

    $output | ForEach-Object { Write-Host $_ }

    Write-Host "`n📄 Summary of the extraction is available in the file:"
    Write-Host "$YELLOW$output_file_path$ENDC"
    Write-Host "`n📂 Full extraction folder path:"
    Write-Host "$YELLOW$extraction_folder$ENDC"
}

# Argument parsing
param (
    [string]$from_date = $null,
    [string]$to_date = $null,
    [string]$search = $null
)

try {
    if ($from_date) { [datetime]::ParseExact($from_date, "yyyy-MM-dd", $null) } 
    if ($to_date) { [datetime]::ParseExact($to_date, "yyyy-MM-dd", $null) }
}
catch {
    Write-Host "Date format must be YYYY-MM-DD."
    exit
}

Main -from_date $from_date -to_date $to_date -search_term $search
