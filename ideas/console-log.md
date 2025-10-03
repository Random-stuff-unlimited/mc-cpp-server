# Console Idea

### Console Section
- **Server Control**: Start, stop, restart server
- **Player Management**: Kick, ban, whitelist players, view player list, view player info
- **World Management**: Load, save, backup world data
- **Configuration**: Modify server settings on runtime
- **Logging section**: View and filter server logs (network, chat, game info (Filter for warning / error))

### Console Architecture
```
ConsoleManager
├── ServerControl
├── PlayerManagement
    	├── player list
         		├── player info / Kick / Ban / Whitelist
        ├── add player to Whitelist (if enabled)
├── WorldManagement (soon)
		├── world list
         		├── world info / Load / Save / Backup
        ├── world info
├── Configuration (soon)
  		├── config list
         		├── config info / Load / Save / Backup
├── LoggingSection
  		├── log network / all / chat / gameinfo
    	├── filter (error / warning / info)
```

# Logging Idea

1 file network (all error / warning / info)
1 file gameinfo (all error / warning / info)
