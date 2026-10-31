# Repositories and branches

The project lives on two servers with the same content:

| Remote | Server | Role |
|---|---|---|
| `origin` | Gitea | main work |
| `github` | GitHub | public state, outside contributions |

| Branch | Content |
|---|---|
| `dev` | ongoing development, commits and pull requests go here |
| `main` | tested state, default branch |

New clones need both remotes:
```powershell
git remote add github https://github.com/SupperCraft5000/Fallout-Together-Reborn.git
```

## Working on Gitea
1. Work on `dev` and push to `origin`.
2. `scripts\sync-repos.ps1` brings `dev` to GitHub.
3. After a passed test, bring `dev` into `main` by fast-forward and sync again:
```powershell
git switch main
git merge --ff-only dev
git push origin main
git switch dev
scripts\sync-repos.ps1
```

## Contributions on GitHub
1. Pull requests go to `dev` and are merged there.
2. `scripts\sync-repos.ps1` brings the result to Gitea.
3. Update locally with `git pull --ff-only origin dev`.

## When both sides have new commits
The script then transfers nothing for that branch. Solution, with `dev` as the example:
```powershell
git fetch --no-tags github
git switch dev
git pull --ff-only origin dev
git merge github/dev
git push origin dev
scripts\sync-repos.ps1
```
Never sync with `--force`.

Do not push upstream tags. They belong to Tilted Online and would otherwise look like our own versions.
