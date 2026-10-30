# Hält main und dev auf Gitea (origin) und GitHub (github) gleich, in beide Richtungen.
# Überträgt nur Fast-Forwards. Haben beide Seiten neue Commits, bricht es für diesen Branch ab,
# dann lokal zusammenführen (siehe docs/REPOSITORIES.md) und erneut starten.
$Root = Split-Path $PSScriptRoot
$conflict = $false

git -C $Root fetch -q --no-tags origin
if ($LASTEXITCODE) { throw 'Abruf von Gitea fehlgeschlagen' }
git -C $Root fetch -q --no-tags github
if ($LASTEXITCODE) { throw 'Abruf von GitHub fehlgeschlagen' }

foreach ($branch in 'main', 'dev') {
    $gitea = git -C $Root rev-parse "refs/remotes/origin/$branch"
    $hub = git -C $Root rev-parse "refs/remotes/github/$branch"

    if ($gitea -eq $hub) {
        Write-Host "${branch}: gleich"
        continue
    }

    git -C $Root merge-base --is-ancestor $gitea $hub
    if ($LASTEXITCODE -eq 0) {
        Write-Host "${branch}: GitHub ist neuer, übertrage nach Gitea"
        git -C $Root push origin "refs/remotes/github/${branch}:refs/heads/$branch"
        if ($LASTEXITCODE) { throw "Push von $branch nach Gitea fehlgeschlagen" }
        continue
    }

    git -C $Root merge-base --is-ancestor $hub $gitea
    if ($LASTEXITCODE -eq 0) {
        Write-Host "${branch}: Gitea ist neuer, übertrage nach GitHub"
        git -C $Root push github "refs/remotes/origin/${branch}:refs/heads/$branch"
        if ($LASTEXITCODE) { throw "Push von $branch nach GitHub fehlgeschlagen" }
        continue
    }

    Write-Host "${branch}: beide Seiten haben neue Commits, bitte zusammenführen"
    $conflict = $true
}

if ($conflict) { exit 1 }
