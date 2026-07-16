# Packs, free textures, backup

## Restore point (saved)

```
C:\UE\RebelstarDelta_BACKUP_pre_MoonPack_20260716_194102
```

Close the Editor, then copy folders from that backup over `C:\UE\RebelstarDelta` if you need to roll back before Moon Pack / Regolith Pack work.

## Buy when ready (Fab)

| Pack | Link |
|------|------|
| **Sci-Fi Base: Moon Theme** | https://www.fab.com/listings/bdfa6ceb-abf9-4421-97ad-00d6246a3cb9 |
| **Moon + Mars bundle** | https://www.fab.com/listings/ac36a82c-12e6-4b58-b61c-ed86749d4431 |
| **Megascans regolith / rocks** | Open **Content Drawer → Fab**, search: `regolith`, `moon rock`, `lunar`, `basalt`, `crater`. Free Quixel assets with UE login. Bridge/Fab catalog: https://www.fab.com/ |

## Free textures already downloaded

```
C:\UE\RebelstarDelta\Content\RawTextures\
  moon_2k_sss.jpg      — Solar System Scope free moon
  moon_texture.jpg     — NASA / Wikimedia moon
  moon_color_2k.tif    — NASA CGI Moon Kit 2k
  earth_2k_day.jpg     — free Earth day map
  earth_bluemarble.jpg — NASA Blue Marble
  milkyway_2k.jpg      — free milky way
```

On **Editor open**, `Content/Python/init_unreal.py` imports these into `/Game/RD/Textures` and builds materials under `/Game/RD/Materials`.  
C++ then uses them for ground / Earth / sky when present.

## Access needed from you

1. Buy/download **Moon theme** into this project (Fab → Add to project → RebelstarDelta).  
2. Add **Megascans** rocks/regolith the same way (free with Epic account).  
3. Tell the agent when those folders appear under `Content/` so meshes can replace greybox.

No extra keys required for NASA free textures.
