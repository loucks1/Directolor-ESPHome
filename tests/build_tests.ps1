# PowerShell script to compile and run the mock tests
# Note: Requires a C++ compiler like g++ in your PATH

$Compiler = "g++"

if (Get-Command $Compiler -ErrorAction SilentlyContinue) {
    Write-Host "Compiling tests..."
    
    $SourceFiles = @(
        "test_directolor_radio.cpp",
        "..\components\directolor_radio\directolor_radio.cpp",
        "..\components\directolor_radio\payload_queue.cpp"
    )
    
    $IncludeArgs = "-I.\mocks"
    
    $Command = "$Compiler -std=c++14 $IncludeArgs " + ($SourceFiles -join " ") + " -o test_runner.exe"
    Invoke-Expression $Command
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Compilation successful. Running tests:"
        .\test_runner.exe
    } else {
        Write-Host "Compilation failed."
    }
} else {
    Write-Host "C++ compiler ($Compiler) not found in PATH."
    Write-Host "Please install MinGW/GCC or use a Linux environment to run these tests."
}
