# Contributing

Getting started: [AGENTS.md](AGENTS.md), [BUILD.md](BUILD.md), [STATUS.md](STATUS.md).

## Reporting a bug
The quickest way is to [open an issue](https://github.com/SupperCraft5000/Fallout-Together-Reborn/issues/new/choose). The template asks for everything needed, in English and German.
Please include:
- Build ID, shown at the top of the starter
- Fallout 4 version, host or player, chosen options
- What happened, what should have happened, the time
- `tp_client.log` from the PC where it happened, ideally also from the other PC, see [DEBUGGING.md](DEBUGGING.md)
- For crashes, the address from the message. Dumps are around 2 GB and can contain memory contents, so do not attach them publicly

Check the logs before uploading. Save names contain your character's name.

## Contributing code
- Please send pull requests to `dev`, not `main`, see [REPOSITORIES.md](REPOSITORIES.md)
- Rules and pitfalls are in [AGENTS.md](AGENTS.md)
- Code comments in English, documentation and scripts in German. The English documentation in `docs/en` mirrors the German one, so update both
- Do not commit game files, exe files, files derived from them or Address Library files
- When changing game addresses, follow the report in [PORTING.md](PORTING.md)
