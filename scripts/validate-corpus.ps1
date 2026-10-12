param(
    [string[]]$PdfPaths=@(),
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$BuildDirectory='build',
    [string]$OutputDirectory,
    [switch]$Stress,
    [switch]$Poppler
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$exe=Join-Path $root "$BuildDirectory/corpus_smoke.exe"
if(!(Test-Path -LiteralPath $exe)) { throw 'Compilá el proyecto primero con scripts/build.ps1.' }
if($Poppler) {
    foreach($command in @('pdfinfo','pdftoppm')) {
        if(!(Get-Command $command -ErrorAction SilentlyContinue)) { throw "Falta $command en PATH para la validación independiente." }
    }
    if(!$OutputDirectory) { $OutputDirectory=Join-Path $root "$BuildDirectory/corpus-verification" }
}
$arguments=@()
if($OutputDirectory) { $arguments+=@('--output-directory',[IO.Path]::GetFullPath($OutputDirectory)) }
if($Stress) { $arguments+='--stress' }
foreach($path in $PdfPaths) {
    $resolved=(Resolve-Path -LiteralPath $path).Path
    if([IO.Path]::GetExtension($resolved) -ine '.pdf') { throw "Se esperaba un PDF: $resolved" }
    $arguments+=$resolved
}
$previous=$env:QT_QPA_PLATFORM
try {
    $env:QT_QPA_PLATFORM='offscreen'
    $result=& $exe @arguments
    $corpusExit=$LASTEXITCODE
    $result | Write-Output
    if($corpusExit) { throw 'Falló la validación del corpus PDF.' }
    if($Poppler) {
        $outputLine=$result | Where-Object { $_ -like 'OUTPUT *' } | Select-Object -First 1
        if(!$outputLine) { throw 'La herramienta no indicó la carpeta de copias.' }
        $runDirectory=$outputLine.Substring(7)
        $rendered=0
        foreach($pdf in Get-ChildItem -LiteralPath $runDirectory -Filter '*copy*.pdf') {
            $info=& pdfinfo $pdf.FullName 2>> (Join-Path $runDirectory 'poppler.log')
            if($LASTEXITCODE) { throw "Poppler no pudo abrir $($pdf.Name)." }
            $pageLine=$info | Select-String '^Pages:'
            if(!$pageLine) { throw "Poppler no indicó páginas para $($pdf.Name)." }
            $pages=[int](($pageLine.ToString() -split ':')[1].Trim())
            $selected=@(1,[Math]::Max(1,[int][Math]::Ceiling($pages/2)),$pages) | Select-Object -Unique
            foreach($page in $selected) {
                & pdftoppm -f $page -l $page -r 48 -singlefile -png $pdf.FullName (Join-Path $runDirectory "$($pdf.BaseName)-poppler-$page") 2>> (Join-Path $runDirectory 'poppler.log')
                if($LASTEXITCODE) { throw "Falló Poppler: $($pdf.Name), página $page." }
                ++$rendered
            }
        }
        "PASS: $rendered páginas renderizadas con Poppler; ver PNG y poppler.log en $runDirectory"
    }
} finally { $env:QT_QPA_PLATFORM=$previous }
