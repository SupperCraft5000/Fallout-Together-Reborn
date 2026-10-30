# Repositories und Branches

Das Projekt liegt auf zwei Servern mit gleichem Inhalt:

| Remote | Server | Rolle |
|---|---|---|
| `origin` | Gitea | Hauptarbeit |
| `github` | GitHub | öffentlicher Stand, Beiträge von außen |

| Branch | Inhalt |
|---|---|
| `dev` | laufende Entwicklung, hierhin gehen Commits und Pull Requests |
| `main` | getesteter Stand, Standard-Branch |

Neue Clones brauchen beide Remotes:
```powershell
git remote add github https://github.com/SupperCraft5000/Fallout-Together-Reborn.git
```

## Arbeit auf Gitea
1. Auf `dev` arbeiten und nach `origin` pushen.
2. `scripts\sync-repos.ps1` bringt `dev` nach GitHub.
3. Nach einem bestandenen Test `dev` per Fast-Forward nach `main` holen und wieder synchronisieren:
```powershell
git switch main
git merge --ff-only dev
git push origin main
git switch dev
scripts\sync-repos.ps1
```

## Beiträge auf GitHub
1. Pull Requests gehen nach `dev` und werden dort zusammengeführt.
2. `scripts\sync-repos.ps1` holt das Ergebnis nach Gitea.
3. Lokal aktualisieren mit `git pull --ff-only origin dev`.

## Wenn beide Seiten neue Commits haben
Das Skript überträgt dann nichts für diesen Branch. Lösung mit `dev` als Beispiel:
```powershell
git fetch --no-tags github
git switch dev
git pull --ff-only origin dev
git merge github/dev
git push origin dev
scripts\sync-repos.ps1
```
Nie mit `--force` synchronisieren.

Upstream-Tags nicht pushen. Sie gehören zu Tilted Online und sehen sonst wie eigene Versionen aus.
