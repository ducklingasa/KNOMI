import unittest

from tools.derive_prototype import Deriver


class DeriverTests(unittest.TestCase):
    def setUp(self):
        self.model = Deriver()
        self.model.m = {
            "state": 1,
            "layer": 0,
            "printProgress": 0,
            "targetNozzleTemp": 200,
            "nozzleTemp": "199.99",
            "targetBedTemp0": 50,
            "bedTemp0": "49.99",
        }

    def assert_state(self, expected_state, expected_sub, **update):
        self.model.m.update(update)
        self.assertEqual(self.model.derive(), expected_state)
        self.assertEqual(self.model.sub, expected_sub)

    def test_nozzle_priority_and_exact_first_reach(self):
        self.assert_state("PROCESSING", "Nozzle heating")
        self.assert_state("PROCESSING", "Bed heating", nozzleTemp="200.00")
        self.assert_state("PROCESSING", "Preparing", bedTemp0="50.00")

    def test_above_target_also_finishes_heating(self):
        self.assert_state("PROCESSING", "Preparing", nozzleTemp="200.01", bedTemp0="50.01")

    def test_temperature_dips_do_not_restart_finished_heating(self):
        self.assert_state("PROCESSING", "Preparing", nozzleTemp=200, bedTemp0=50)
        self.assert_state("PROCESSING", "Preparing", nozzleTemp=198, bedTemp0=48)

    def test_target_change_rearms_only_that_heater(self):
        self.assert_state("PROCESSING", "Preparing", nozzleTemp=200, bedTemp0=50)
        self.assert_state("PROCESSING", "Bed heating", targetBedTemp0=60)
        self.assert_state("PROCESSING", "Nozzle heating", targetNozzleTemp=210)
        self.assert_state("PROCESSING", "Bed heating", nozzleTemp=210)
        self.assert_state("PROCESSING", "Preparing", targetBedTemp0=40)

    def test_target_off_then_on_rearms_heating(self):
        self.assert_state("PROCESSING", "Preparing", nozzleTemp=200, bedTemp0=50)
        self.assert_state("PROCESSING", "Preparing", targetNozzleTemp=0, nozzleTemp=190)
        self.assert_state("PROCESSING", "Nozzle heating", targetNozzleTemp=200)

    def test_self_test_tracks_heaters_without_losing_step(self):
        self.assert_state("SELF_TESTING", "step 2; Nozzle heating", withSelfTest=2)
        self.assert_state("SELF_TESTING", "step 2; Bed heating", nozzleTemp=200)
        self.assert_state("SELF_TESTING", "step 2", bedTemp0=50)

    def test_unknown_actual_does_not_finish_heating(self):
        self.assert_state("PROCESSING", "Bed heating", nozzleTemp="")
        self.assertFalse(self.model.heaters["Nozzle"][1])
        self.assert_state("PROCESSING", "Nozzle heating", nozzleTemp="199.99")

    def test_placeholder_dump_does_not_finish_heating(self):
        self.assert_state("NOT_READY", "", connect=0, nozzleTemp=201, bedTemp0=51)
        self.assertEqual(self.model.heaters, {})
        self.assert_state("PROCESSING", "Nozzle heating", connect=1, nozzleTemp=199)

    def test_new_job_rearms_same_target(self):
        self.assert_state("PROCESSING", "Preparing", printFileName="part.gcode", printStartTime=1,
                          nozzleTemp=200, bedTemp0=50)
        self.assert_state("PROCESSING", "Nozzle heating", printStartTime=2, nozzleTemp=199)

    def test_idle_operation_releases_completion_latch(self):
        for device_state, label in ((3, "Bed leveling"), (4, "Input shaping"), (7, "Homing")):
            with self.subTest(device_state=device_state):
                self.assert_state("COMPLETED", "", state=2, printProgress=100,
                                  targetNozzleTemp=0, targetBedTemp0=0)
                self.assert_state("COMPLETED", "", state=0, deviceState=0)
                self.assert_state("PROCESSING", label, deviceState=device_state)
                self.assertIsNone(self.model.latch)
                self.assert_state("IDLE", "", deviceState=0)

    def test_calibration_keeps_primary_label_while_heating(self):
        self.assert_state("PROCESSING", "Bed leveling; Nozzle heating", state=0, deviceState=3)
        self.assert_state("PROCESSING", "Bed leveling; Bed heating", nozzleTemp=200)

    def test_input_shaping_does_not_override_active_print(self):
        self.assert_state("PRINTING", "", layer=1, deviceState=4, nozzleTemp=200, bedTemp0=50)

    def test_resume_keeps_primary_label_while_heating(self):
        self.assert_state("PROCESSING", "Resuming; Nozzle heating", state=8)

    def test_nonzero_error_overrides_every_ready_printer_state(self):
        for raw_state in range(10):
            with self.subTest(raw_state=raw_state):
                self.assert_state("ERROR", "", state=raw_state, withSelfTest=1,
                                  err={"errcode": 101, "key": 104, "value": ""})

    def test_paused_error_is_visible_until_printer_clears_it(self):
        self.assert_state("ERROR", "", state=5, err={"errcode": 101, "key": 104})
        self.assert_state("PAUSED", "", err={"errcode": 0, "key": 0})

    def test_placeholder_error_is_not_valid_printer_data(self):
        self.assert_state("NOT_READY", "", connect=0, state=5,
                          err={"errcode": 101, "key": 104})
        self.assert_state("ERROR", "", connect=1)


if __name__ == "__main__":
    unittest.main()