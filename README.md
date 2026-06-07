# real_time_ray_tracing_v2

Silnik renderowania w czasie rzeczywistym z dwoma backendami: **raster** (Vulkan graphics) i **ray tracing** (Vulkan RT). Sceny ładują się z plików JSON w katalogu `scenes/`.

## Wymagania

- C++20, CMake 4.2+
- Vulkan SDK (w tym `slangc` do kompilacji shaderów)
- GPU z obsługą RT (opcjonalnie — bez RT aplikacja startuje w trybie raster)

## Budowanie i uruchomienie

```bash
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug --target real_time_ray_tracing_v2
```

Uruchom binarkę z katalogu build (obok `scenes/`, `images/`, `models/` skopiowanych przez CMake):

```bash
./cmake-build-debug/real_time_ray_tracing_v2
```

Domyślnie startuje backend **raster** (`main.cpp`). Przełączenie na RT w runtime: **F7** (jeśli GPU wspiera rozszerzenia RT).

---

## Sterowanie (klawiszologia)

Skróty działają przy aktywnym oknie aplikacji (focus). Większość akcji jest blokowana podczas trwającego benchmarku (F5) lub przeładowania sceny / backendu.

### Ruch kamery (free look)

| Klawisz | Akcja |
|---------|--------|
| **W** | Ruch do przodu |
| **S** | Ruch do tyłu |
| **A** | Ruch w lewo |
| **D** | Ruch w prawo |
| **Spacja** | Ruch w górę |
| **Ctrl (lewy)** | Ruch w dół |
| **Mysz** | Obrót kamery (yaw / pitch, pitch ograniczony do ±89°) |

Ruch kamery jest **wyłączony** podczas benchmarku (kamera jedzie po zdefiniowanej ścieżce) oraz gdy włączona jest blokada (**L**).

### Kamera — ustawienia

| Klawisz | Akcja |
|---------|--------|
| **L** | Włącza / wyłącza blokadę ruchu kamery (WASD, spacja, Ctrl, mysz). Presety (**TAB**) i log pozycji (**P**) działają nadal. |
| **TAB** | Przełącza preset kamery zdefiniowany w JSON sceny (jeśli scena ma `camera_presets`). |
| **P** | Wypisuje w konsoli aktualną pozycję i kąty Eulera kamery (do kopiowania do JSON). |

### Sceny

| Klawisz | Scena |
|---------|--------|
| **F1** | Test |
| **F2** | GraphicsTest |
| **F3** | StressTest |

Przełączenie sceny przeładowuje geometrię i tekstury na GPU (krótka pauza renderu). Nie można zmieniać sceny w trakcie benchmarku.

### Benchmark wydajności

| Klawisz | Akcja |
|---------|--------|
| **F5** | Start / stop benchmarku |

- Domyślny czas pomiaru: **180 s** (z pola `benchmark.duration_seconds` w JSON sceny).
- Pierwsze **30 klatek** to rozgrzewka — nie wchodzą do statystyk.
- Po zakończeniu wynik zapisywany jest jako JSON w katalogu `measurements/` obok binarki.
- Na scenach z polem `benchmark_path` kamera automatycznie animuje się między punktami A→B podczas pomiaru.

### Backend renderera

| Klawisz | Akcja |
|---------|--------|
| **F7** | Przełącza **raster ↔ ray tracing** |

Dostępne tylko gdy GPU wspiera wymagane rozszerzenia Vulkan RT. W przeciwnym razie aplikacja pozostaje w trybie raster.

### StressTest (tylko scena F3)

| Klawisz | Akcja |
|---------|--------|
| **[** | Zmniejsza liczbę instancji (krok ze `stress.step` w JSON) |
| **]** | Zwiększa liczbę instancji |

Limity: `stress.min_count` … `stress.max_count` w `scenes/stress_test.json`. Przebudowa sceny trwa chwilę — liczba obiektów rośnie/maleje z losowym rozmieszczeniem (stały seed).

---

## Sceny i assety

| Plik | Opis |
|------|------|
| `scenes/test_scene.json` | Prosta scena testowa (sześcian + teksturowana podłoga) |
| `scenes/graphics_test.json` | Scena porównawcza graficzna (DiffImg) + model GLTF |
| `scenes/stress_test.json` | Duża liczba instancji, regulacja `[` / `]` |

Assety (`images/`, `models/`) są kopiowane do katalogu build przez CMake.

## Logi w konsoli

Prefiksy: `[Engine]`, `[Scene]`, `[Camera]`, `[Benchmark]`, `[Stress]`, `[Renderer]`. Przy starcie wypisywane są GPU, aktywny backend i dostępność przełącznika F7.
