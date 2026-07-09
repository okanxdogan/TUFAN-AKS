# can_replay — offline CAN sniffer oturumu regresyon aracı

Kaydedilmiş bir CAN sniffer oturumunu (metin log) **gerçek** saf parser'lardan
(`lib/CanParse/CanParse.cpp`) geçirir ve invariant'ları doğrular. Donanım
gerekmez; native testlerin kullandığı `test/support/idf_stubs` ile derlenir.

## Derleme

```sh
tools/can_replay/build.sh
```

## Kullanım

```sh
# Oturum 2 (idle) assert'leri açık — varsayılan:
tools/can_replay/can_replay <log-yolu>

# Yalnızca genel kontroller (çökmezlik + bilinen ID listesi), başka
# oturumlar için:
tools/can_replay/can_replay --generic <log-yolu>

# Hızlı deneme (repodaki küçük örnek fixture ile):
tools/can_replay/can_replay tools/can_replay/fixtures/sample-session.log
```

Çıkış kodu `0` = PASS, `1` = ihlal/bozuk satır var, `2` = kullanım hatası.

## Log formatı

Boş satırlar ve `#` yorumları atlanır. Her frame bir satır:

```
EXT | 0xE000     | 8 | FF FF 03 16 0F 5E 09 71
STD | 0x000      | 8 | 00 00 00 00 00 00 00 00
```

## Doğrulanan invariant'lar (Oturum 2, paket idle)

- Tüm `0xE000` frame'lerinde `packV == 790` (79.0 V) ve `byte[2:3] == 0x0316`
- `0xE000` ham akım int16 değeri `{-1, -2}` kümesinde (DOĞRULANDI, çarpan 0.1A)
- Tüm `0x1806E5F4` frame'lerinde setpoint sabit: 88.0 V / 100.0 A
- Bilinen 10 ID (`E000..E005`, `E032`, `E033`, `1806E5F4`, STD `0x000`)
  dışında ID gelirse raporlanır
- Tüm frame'ler ilgili parser'dan geçirilir — stub ID'ler dahil; amaç
  "parser tüm oturumu çökmeden tüketiyor" garantisi

## Gerçek log dosyası

Tam oturum kaydı (ör. `esp32-session.log`, ~24k satır) repoya **gömülmez** —
`tools/can_replay/` altındaki `*.log` dosyaları gitignore'dadır; yalnızca
`fixtures/sample-session.log` örneği versiyonlanır. Gerçek logu herhangi bir
yerel yola koyup argüman olarak verin.
