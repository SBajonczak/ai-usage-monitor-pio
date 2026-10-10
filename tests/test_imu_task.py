from pathlib import Path
import unittest
P=Path(__file__).resolve().parents[1]
M=(P/'src/main.cpp').read_text()
class IMUTaskTests(unittest.TestCase):
 def test_gyro_is_separate_task(self):
  self.assertIn('xTaskCreatePinnedToCore(', M)
  self.assertIn('vTaskDelayUntil(', M)
  self.assertIn('motionTask, "motion",', M)
 def test_bus_serialization(self):
  self.assertIn('xSemaphoreTake(i2cMutex,', M)
  self.assertIn('xSemaphoreGive(i2cMutex)', M)
 def test_prediction_and_stability(self):
  self.assertIn('motion_prediction::predict(', M)
  self.assertIn('AUTO_ROTATION_STILL_MS', M)
 def test_no_old_sampling_in_panel_loop(self):
  panel=M.split('static void updatePhysicalPanel() {', 1)[1].split('// ---------- Usage state ----------',1)[0]
  self.assertNotIn('sampleMotion();', panel)
if __name__=='__main__': unittest.main()
