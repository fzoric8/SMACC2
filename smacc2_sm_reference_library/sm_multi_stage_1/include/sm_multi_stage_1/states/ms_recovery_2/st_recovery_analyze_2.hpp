// Copyright 2021 RobosoftAI Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

namespace sm_multi_stage_1
{
// STATE DECLARATION
struct StRecoveryAnalyze2 : smacc2::SmaccState<StRecoveryAnalyze2, MsRecovery2>
{
  using SmaccState::SmaccState;

  // DECLARE CUSTOM OBJECT TAGS
  struct TIMEOUT : ABORT{};
  struct NEXT : SUCCESS{};
  struct PREVIOUS : ABORT{};

  // TRANSITION TABLE
  typedef mpl::list<

    Transition<EvTimer<CbTimerCountdownOnce, OrTimer>, StRecoveryBifurcate2, TIMEOUT>,
    // Transition<smacc2::EvTopicMessage<CbWatchdogSubscriberBehavior, OrSubscriber>, SsMode1SequenceA>,
    // Keyboard events
    Transition<EvKeyPressN<CbDefaultKeyboardBehavior, OrKeyboard>, StRecoveryBifurcate2, SUCCESS>
    // Transition<EvKeyPressB<CbDefaultKeyboardBehavior, OrKeyboard>, SsMode1SequenceB, BUILD>,
    // Transition<EvKeyPressC<CbDefaultKeyboardBehavior, OrKeyboard>, SsPCCycle, ATTACK>

    >reactions;

  // STATE FUNCTIONS
  static void staticConfigure()
  {
    configure_orthogonal<OrTimer, CbTimerCountdownOnce>(50);
    configure_orthogonal<OrSubscriber, CbWatchdogSubscriberBehavior>();
    configure_orthogonal<OrUpdatablePublisher, CbDefaultPublishLoop>();
    configure_orthogonal<OrKeyboard, CbDefaultKeyboardBehavior>();
  }

  void runtimeConfigure() {}

  void onEntry() { RCLCPP_INFO(getLogger(), "On Entry!"); }

  void onExit() { RCLCPP_INFO(getLogger(), "On Exit!"); }
};
}  // namespace sm_multi_stage_1