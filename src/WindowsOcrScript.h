#pragma once
// Windows PowerShell 5.1 bridges WinRT for the MinGW build, without extra DLLs.
inline constexpr const char* windowsOcrScript=R"PS(
param([string]$ImagePath)
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
[Console]::OutputEncoding=New-Object System.Text.UTF8Encoding($false)
try {
 Add-Type -AssemblyName System.Runtime.WindowsRuntime
 $null=[Windows.Storage.StorageFile,Windows.Storage,ContentType=WindowsRuntime]
 $null=[Windows.Graphics.Imaging.BitmapDecoder,Windows.Graphics.Imaging,ContentType=WindowsRuntime]
 $null=[Windows.Media.Ocr.OcrEngine,Windows.Foundation,ContentType=WindowsRuntime]
 $asTask=([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { $_.Name -eq 'AsTask' -and $_.IsGenericMethod -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' } | Select-Object -First 1)
 function Await($operation,$type) {
  $task=$asTask.MakeGenericMethod($type).Invoke($null,@($operation))
  $task.GetAwaiter().GetResult()
 }
 $engine=[Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages()
 if(!$engine) { throw 'No OCR language is installed. Install an OCR language in Windows Settings > Time & language.' }
 $ImagePath=[System.IO.Path]::GetFullPath($ImagePath)
 $file=Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync($ImagePath)) ([Windows.Storage.StorageFile])
 $stream=Await ($file.OpenAsync([Windows.Storage.FileAccessMode]::Read)) ([Windows.Storage.Streams.IRandomAccessStream])
 try {
  $decoder=Await ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)) ([Windows.Graphics.Imaging.BitmapDecoder])
  $bitmap=Await ($decoder.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap])
  try {
   if($bitmap.PixelWidth -gt [Windows.Media.Ocr.OcrEngine]::MaxImageDimension -or $bitmap.PixelHeight -gt [Windows.Media.Ocr.OcrEngine]::MaxImageDimension) { throw 'OCR image too large.' }
   $result=Await ($engine.RecognizeAsync($bitmap)) ([Windows.Media.Ocr.OcrResult])
   $words=@(); $line=0
   foreach($row in $result.Lines) {
    foreach($word in $row.Words) {
     $r=$word.BoundingRect
     $words+=@{text=$word.Text;x=$r.X;y=$r.Y;w=$r.Width;h=$r.Height;line=$line}
    }
    $line++
   }
   ConvertTo-Json -Compress -Depth 5 -InputObject @{words=@($words);language=$engine.RecognizerLanguage.LanguageTag}
  } finally { $bitmap.Dispose() }
 } finally { $stream.Dispose() }
} catch { [Console]::Error.WriteLine($_.Exception.Message); exit 1 }
)PS";
