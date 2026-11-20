from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
class StorageGuardTests(unittest.TestCase):
    def test_mount_formats_only_verified_blank_partition_and_fs_adapter_is_shared(self):
        ino=(ROOT/'firmware/bytereel_2026/bytereel_2026.ino').read_text()
        guard=(ROOT/'firmware/bytereel_2026/storage_boot_guard.h').read_text()
        harness=(ROOT/'littlefs_fault_test/littlefs_fault_test.ino').read_text()
        self.assertIn('LittleFS.begin(false,"/littlefs",4,"spiffs")',ino)
        self.assertIn('esp_partition_read(p,off,buf,n)',ino)
        self.assertIn('BR_STORAGE_RECOVERY_FORMAT_BLANK',guard)
        self.assertIn('else if(!LittleFS.format())',ino)
        self.assertIn('br_storage_recovery_checked(mounted,stopped,check)',ino)
        self.assertNotIn('formatOnFail',ino)
        self.assertIn('br_commit_slot(store,id',ino)
        self.assertIn('BrLittleFsStore store(LittleFS,"/slots",storageHealthy)',ino)
        self.assertIn('br_commit_slot(store,0,OLD_PAIR',harness)
        self.assertIn('begin(false,"/littlefs",4,"spiffs")',harness)
        self.assertIn('if(LittleFS.exists(TEST_DIR))',harness)
    def test_input_expander_remains_read_only(self):
        ino=(ROOT/'firmware/bytereel_2026/bytereel_2026.ino').read_text()
        begin=ino.index('static bool i2cReadInputs')
        end=ino.index('static void ledWrite',begin)
        reader=ino[begin:end]
        self.assertNotIn('Wire.write',reader)
        self.assertNotIn('Wire.beginTransmission',reader)
        self.assertNotIn('Wire.endTransmission',reader)
        self.assertIn('Wire.requestFrom(INPUT_ADDR,requested)',reader)
        self.assertNotIn('beginTransmission(INPUT_ADDR)',ino)
        self.assertIn('static constexpr uint8_t LED_ADDR=0x20, INPUT_ADDR=0x22',ino)
        self.assertIn('I2C_SDA_PIN=21, I2C_SCL_PIN=22',ino)
        self.assertIn('I2C_FREQUENCY_HZ=10000',ino)
        for key in ('inputReadErrors','ledWriteErrors','lastRead','lastError',
                    'lastValidInput','activeControls','frequencyHz'):
            self.assertIn(key,ino)
    def test_usb_debug_and_peripheral_uart_are_separate(self):
        ino=(ROOT/'firmware/bytereel_2026/bytereel_2026.ino').read_text()
        self.assertIn('Serial.begin(115200)',ino)
        self.assertIn('Serial2.begin(UART_BAUD,SERIAL_8N1,PIN_UART_RX,PIN_UART_TX)',ino)
        self.assertIn('Serial2.write(txb)',ino)
        self.assertNotIn('Serial.write(',ino)
        self.assertNotIn('Serial.updateBaudRate',ino)
    def test_wifi_only_starts_after_debounced_memory_transition(self):
        ino=(ROOT/'firmware/bytereel_2026/bytereel_2026.ino').read_text()
        self.assertNotIn('webInit();',ino)
        self.assertIn('if(uiModeGate.observe(edit))webModeTransition(!edit)',ino)
        self.assertIn('if(webRunning){const uint32_t httpStarted=micros();http.handleClient()',ino)
        self.assertIn('if(webRoutesConfigured)return',ino)
        worker=ino[ino.index('static void wifiControlTask'):ino.index('static bool i2cReadInputs')]
        self.assertIn('WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF)',worker)
        self.assertIn('wifiTaskReady=xTaskCreatePinnedToCore(wifiControlTask',ino)
        self.assertIn('"WIFI_START"',ino)
        self.assertIn('"WIFI_READY"',ino)
        self.assertIn('"WIFI_STOP"',ino)
    def test_oversized_existing_candidate_is_not_treated_as_missing(self):
        adapter=(ROOT/'firmware/bytereel_2026/littlefs_slot_adapter.h').read_text()
        self.assertIn('lastError="READ_OVERSIZE";return -2',adapter)
        self.assertNotIn('f.close();return -1',adapter)
        store=(ROOT/'firmware/bytereel_2026/slot_store.h').read_text()
        self.assertIn('BR_COMMIT_PHASE_VERIFY_CRC',store)
        self.assertIn('store.lastError', (ROOT/'firmware/bytereel_2026/bytereel_2026.ino').read_text())
if __name__=='__main__':unittest.main()
