# PVB

[English](../README.md) | [Română](docs/README-RO.md)

PVB este un limbaj de programare vizual bazat pe un editor cu blocuri. Programele sunt construite prin conectarea blocurilor în locul scrierii codului, ceea ce facilitează experimentarea, prototiparea și înțelegerea fluxului unui program. Proiectele pot fi transpilate în cod sursă, în prezent fiind suportate limbajele C++ și Python.

Documentația pentru Infoeducație poate fi găsită [aici](docs/PVB_Documentatie_Infoeducatie_2026.pdf)

## Cerințe

### Compilarea PVB

Pentru compilarea aplicației PVB este necesar un compilator compatibil cu C++20 și CMake.

Compilatoare suportate:
- GCC
- Clang
- MSVC

### Rularea programelor generate

Programele generate necesită instrumentele corespunzătoare limbajului folosit:

- Codul C++ necesită un compilator C++ dacă dorești să îl compilezi într-un executabil.
- Codul Python necesită un interpretor Python 3.

PVB nu include aceste instrumente; ele trebuie instalate separat pe sistem.

## Clonare

Clonează repository-ul împreună cu submodulele:

```bash
git clone --recursive https://github.com/mcostn/pvb.git
cd pvb
```

Dacă ai clonat deja repository-ul fără submodule:

```bash
git submodule update --init --recursive
```

## Compilare

### Windows

Inițializează directorul de build:

```bat
init.bat
```

Compilează proiectul:

```bat
cmake --build build --parallel
```

### Linux

Inițializează directorul de build:

```bash
./init.sh
```

Compilează proiectul:

```bash
cmake --build build --parallel
```

## Opțiuni de configurare

Scripturile de inițializare acceptă următoarele opțiuni:

| Opțiune | Descriere |
|---------|-----------|
| `-d`, `--debug` | Configurează un build Debug (implicit). |
| `-r`, `--release` | Configurează un build Release. |
| `-c`, `--clean` | Șterge directorul de build înainte de configurare. |
| `--reconfigure` | Rulează din nou configurarea CMake. |
