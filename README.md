# Emulator Virtuelne Memorije

## Operativni Sistemi 2

### Odsek za Softversko inženjerstvo, Elektrotehnički fakultet, Univerzitet u Beogradu, 2018.

Projekat je realizovan u programskom jeziku C++ (standard *C++11*).

Detaljan opis projekta nalazi se u fajlu *EmulatorVirtuelneMemorije.pdf*

#### Napomene:
* Ciljani (host) operativni sistem: **Windows 7 32-bitni ili 64-bitni (ili noviji)**
* Ciljana arhitektura: **x86**
* Kako bi bio kompatibilan sa sa bibliotekom *part.lib* izvorni kod projekta trebalo bi da se
  prevede koristeći VC++ kompajler koji dolazi uz razvojno okruženje Visual Studio 2015.
    * Solution Configuration: **Debug**
    * Solution Platform: **x86**
* Biblioteka *part.lib* je implementacija klase Partition.
* Biblioteka *part.lib* koristi standardno C++11 *mutex* zaglavlje - svaka operacija klase
  Partition je *thread-safe*.


