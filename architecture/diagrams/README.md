# Diagramy PlantUML

Źródła: `*.puml` w tym katalogu.

## Diagramy

| Plik | Opis |
|------|------|
| `01_overview.puml` | Wysokopoziomowa architektura silnika |
| `02_engine.puml` | Warstwa engine |
| `03_scene_cpu.puml` | Model sceny (CPU) i ładowanie |
| `04_renderer_facade_di.puml` | Fasada renderera |
| `05_shared_vulkan.puml` | Współdzielona infrastruktura Vulkan |
| `06_raster_backend.puml` | Backend rastrowy |
| `07_ray_tracing_backend.puml` | Backend ray tracing |

## Generowanie wszystkich diagramów

**WSL / Linux / Git Bash** (z katalogu głównego repozytorium):

```bash
chmod +x architecture/generate_diagrams.sh   # raz, po sklonowaniu
./architecture/generate_diagrams.sh
```

Opcje: `--png-only`, `--svg-only`, `--checkonly`, `--help`.

Wynik trafia do `architecture/out/` (katalog `out/` jest ignorowany przez git).

## Generowanie jednego diagramu

Uruchom skrypt powyżej albo ręcznie z katalogu `architecture/diagrams` (ważne: `-o ../out` jest względem tego katalogu):

```bash
cd architecture/diagrams
plantuml -tsvg -tpng -o ../out 01_overview.puml
```

**Sprawdzenie składni:**

```bash
./architecture/generate_diagrams.sh --checkonly
```
