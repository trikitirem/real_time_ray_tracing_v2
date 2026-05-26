# Diagramy PlantUML

Źródła: `*.puml` w tym katalogu.  
Plan, checklista i **konwencje (styling, brak podpisów na strzałkach)** (lokalnie, nie w git): [`docs/DIAGRAM_PLAN.md`](../../docs/DIAGRAM_PLAN.md).

## Generowanie wszystkich diagramów

**WSL / Linux / Git Bash** (z katalogu głównego repozytorium):

```bash
chmod +x architecture/generate_diagrams.sh   # raz, po sklonowaniu
./architecture/generate_diagrams.sh
```

Opcje: `--png-only`, `--svg-only`, `--checkonly`, `--help`.

Wynik trafia do `architecture/out/` (katalog `out/` jest ignorowany przez git).

`99_full_reference.puml` zapisuje pliki jako `real_time_ray_tracing_v2_classes.{png,svg}` (nazwa z `@startuml` w pliku).

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

## Kolejność pracy

Zacznij od `01_overview.puml` — pliki twórz według kolejności w `docs/DIAGRAM_PLAN.md`.

Istniejący pełny diagram referencyjny: `99_full_reference.puml`.
