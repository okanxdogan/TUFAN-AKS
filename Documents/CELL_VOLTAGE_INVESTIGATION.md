# Lithium Balance c-BMS 24 Hücre Voltajı Analizi ve Çözüm Yolları

Bu doküman, şu anda (2026-07) ESP-AKS repomuzda henüz CAN ID'si bilinmeyen ve Nextion HMI ekranında "--" olarak gösterilen 24 hücre voltajı (`TEL_bmsCellVoltages[24]`) verisinin **nasıl bulunacağı** ve **sisteme nasıl entegre edileceği** süreçlerini açıklar.

## Sorunun Tespiti
Şu an elimizdeki sniffer loglarında yalnızca şu CAN ID'ler aktif olarak görülmektedir:
- `0xE000`: Pack Voltajı, Akım, SoC 1/2 (**Çözüldü**)
- `0xE001`: Temperature 1/2 (**Çözüldü**)
- `0xE002`, `0xE003`, `0xE004`, `0xE005`: Multiplex yapı (muhtemelen konfigürasyon veya limit yayınları).
- `0xE032`, `0xE033`: Heartbeat / Reserved (genellikle hep sıfır).
- `0x1806E5F4`: Charger hedef gerilim/akım.

24 hücre voltajı (`24 × 2 byte = 48 byte`) bu frame'lere sığmaz. Hücre verilerinin aktarılabilmesi için en az **6 adet CAN frame** (veya 1 frame'in multiplex edilmiş 6 versiyonu) gereklidir. Bu veri ya mevcut `E002-E005` ID'lerinde gizli bir multiplex mantığı ile aktarılmaktadır ya da BMS CAN konfigürasyonunda **yayına hiç açılmamıştır**.

---

## Yöntem 1: Korelasyon ve "Diagnostic Sniffer" (Black-Box Yaklaşımı)

BMS'i yeniden programlamadan (veya programlayamıyorsak) mevcut akan veride hücre voltajı olup olmadığını bulmanın en güvenilir yolu.

### 1. Diagnostic Sniffer'ı Aktif Etme
ESP-AKS firmware'i içinde bu araştırma için bir sniffer modu geliştirilmiştir.
`ESP_AKS/include/SystemConfig.h` dosyasında şu bayrağı `1` yapın:
```cpp
#define ENABLE_BMS_DIAGNOSTIC_SNIFFER 1
```

Bu mod açıldığında `CanManager.cpp`, `E002-E005` ve `E032-E033` ID'lerinde akan verileri okur, 16-bit Big-Endian (veya duruma göre Little-Endian) çiftleri birleştirip `2.50V - 3.65V` aralığında (2500-3650 mV veya 25000-36500 deciMv) bir değer yakalarsa loglara `---> HÜCRE ADAYI! [bX:Y=değer]` uyarısı basar.

### 2. Fiziksel Korelasyon Testi (Uygulama Adımları)
Hücre voltajlarının dinamik değişimini tetikleyerek sniffer üzerindeki yansımalarını izleyin:

1. **Test Ortamı:** Pili dengesiz (örneğin %20 şarjda) veya şarjın bitmesine yakın bir durumda araca bağlayın.
2. **Sniffer Takibi:** Seri porttan (UART) ESP-AKS loglarını izlemeye başlayın. Diagnostic mod hücre adaylarını basıyor olacaktır.
3. **Yük (Discharge) veya Şarj Uygulama:** 
   - Yüksek akım çeken bir manevra yapın (örneğin motoru yüke bindirin).
   - Veya aracı şarja takın.
4. **Gözlem:** Voltajı değişen hücrelerin CAN byte'ları değişecektir. Hangi ID'nin hangi byte çiftlerinin voltaj karakteristiğine uygun olarak (yük altında düşüp şarjda yükseldiği) hareket ettiğini kaydedin.
5. **Multiplex Çözümü:** Eğer `E002` veya `E004` aynı ID altında farklı zamanlarda farklı değerler basıyorsa (ilk byte 1, 2, 3 gibi artıyorsa), bu bir multiplex frame'dir. Multiplex index byte'ını bulup hücreleri `index * 3` veya `index * 4` formülüyle haritalayın.

---

## Yöntem 2: CREATOR Yazılımı ile Doğrudan Map'leme (White-Box Yaklaşımı)

Eğer Yöntem 1'de hücre voltajları CAN hattında hiç bulunamazsa, BMS konfigürasyonunda hücre voltajı mesajları kapatılmış demektir. Lithium Balance c-BMS'in PC yazılımı olan **CREATOR** kullanılarak bu veriler yayına açılmalıdır.

### Adımlar:
1. **CREATOR ile Bağlantı:** c-BMS'i CAN-USB dönüştürücüsü (örneğin Kvaser veya Ixxat) ile bilgisayara bağlayıp CREATOR yazılımını açın.
2. **Object Dictionary (CANopen):** BMS'in CANopen obje sözlüğüne gidin. Hücre voltajlarını tutan diziyi (örneğin `Cell Voltage 1..24`) bulun.
3. **PDO Mapping (Yayına Açma):**
   - Boş bir Transmit PDO (TPDO) kanalı seçin (Örn: TPDO 5, 6, 7). `IDs.md`'ye göre BMS'te 40 adet TPDO bulunmaktadır.
   - Her bir TPDO 8 byte (4 hücre) alabilir. 24 hücre için toplam **6 TPDO**'ya ihtiyacınız olacak.
   - TPDO1 (ID örn: 0x1A0): Hücre 1, 2, 3, 4 haritalayın.
   - TPDO2 (ID örn: 0x1A1): Hücre 5, 6, 7, 8 haritalayın.
   - (Bu işlemi Hücre 24'e kadar 6 adet TPDO için tekrarlayın).
4. **Yayın Periyodu (Event Timer):** Bu 6 TPDO'nun yayın hızını (Event Timer) örneğin 500ms (veya 1000ms) olarak ayarlayın.
5. **Kaydet ve Yeniden Başlat:** Konfigürasyonu BMS'e (NVM) yazın.
6. **ESP-AKS'e Entegrasyon:** CREATOR'da atadığınız yeni CAN ID'leri ESP-AKS içindeki `CanManager.cpp` ve `SystemConfig.h`'a ekleyip `parseLbBmsCellVoltage` isimli yeni bir parser yazın.

---

## Mutlak Kural: Doğrulanana Kadar HMI'a Veri Uydurmamak

Mevcut yazılımda `HMI_CELL_VOLTAGE_SOURCE_VERIFIED = false` (veya testlerde `false` dönecek şekilde kurgulanmış) olduğu için Nextion ekranda hücre voltajı barları boş ve değerleri `--` olarak görünmektedir. 

- **KESİNLİKLE YAPILMAMASI GEREKENLER:** Pack voltajını (`TEL_bmsPackVoltageDeciV`) 24'e bölüp hücrelere suni ortalama değerler YAZMAYIN. Bu (G8/M4) hatasıdır ve bataryada arızalı/dengesiz bir hücre olduğunda bunu ekranda GİZLEYEREK yangın veya arıza riskini artırır.
- **YAPILMASI GEREKEN:** `VehicleData.h` içindeki `TEL_bmsCellVoltages[24]` dizisi sıfırda kalmaya devam etmelidir. Yukarıdaki yöntemlerden biriyle verinin gerçek CAN kaynağı çözülene kadar hücre voltajları ekranda `--` kalacaktır. Çözüldüğünde diziyi doldurun ve HMI gönderim bayrağını `true` yapın.
