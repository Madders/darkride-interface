// Unity's humanoid tables (HumanTrait and the humanoid Avatar's internals), as Unity 6000.3 reports them.
// Bones Unity reports no signs for (LeftToes, RightToes, Jaw, UpperChest) take +1 signs.
const MuscleTrait MUSCLES[MUSCLE_COUNT] = {
    { "Spine Front-Back", 7, 2, -40.0f, 40.0f },
    { "Spine Left-Right", 7, 1, -40.0f, 40.0f },
    { "Spine Twist Left-Right", 7, 0, -40.0f, 40.0f },
    { "Chest Front-Back", 8, 2, -40.0f, 40.0f },
    { "Chest Left-Right", 8, 1, -40.0f, 40.0f },
    { "Chest Twist Left-Right", 8, 0, -40.0f, 40.0f },
    { "UpperChest Front-Back", 54, 2, -20.0f, 20.0f },
    { "UpperChest Left-Right", 54, 1, -20.0f, 20.0f },
    { "UpperChest Twist Left-Right", 54, 0, -20.0f, 20.0f },
    { "Neck Nod Down-Up", 9, 2, -40.0f, 40.0f },
    { "Neck Tilt Left-Right", 9, 1, -40.0f, 40.0f },
    { "Neck Turn Left-Right", 9, 0, -40.0f, 40.0f },
    { "Head Nod Down-Up", 10, 2, -40.0f, 40.0f },
    { "Head Tilt Left-Right", 10, 1, -40.0f, 40.0f },
    { "Head Turn Left-Right", 10, 0, -40.0f, 40.0f },
    { "Left Eye Down-Up", 21, 2, -10.0f, 15.0f },
    { "Left Eye In-Out", 21, 1, -20.0f, 20.0f },
    { "Right Eye Down-Up", 22, 2, -10.0f, 15.0f },
    { "Right Eye In-Out", 22, 1, -20.0f, 20.0f },
    { "Jaw Close", 23, 2, -10.0f, 10.0f },
    { "Jaw Left-Right", 23, 1, -10.0f, 10.0f },
    { "Left Upper Leg Front-Back", 1, 2, -90.0f, 50.0f },
    { "Left Upper Leg In-Out", 1, 1, -60.0f, 60.0f },
    { "Left Upper Leg Twist In-Out", 1, 0, -60.0f, 60.0f },
    { "Left Lower Leg Stretch", 3, 2, -80.0f, 80.0f },
    { "Left Lower Leg Twist In-Out", 3, 0, -90.0f, 90.0f },
    { "Left Foot Up-Down", 5, 2, -50.0f, 50.0f },
    { "Left Foot Twist In-Out", 5, 1, -30.0f, 30.0f },
    { "Left Toes Up-Down", 19, 2, -50.0f, 50.0f },
    { "Right Upper Leg Front-Back", 2, 2, -90.0f, 50.0f },
    { "Right Upper Leg In-Out", 2, 1, -60.0f, 60.0f },
    { "Right Upper Leg Twist In-Out", 2, 0, -60.0f, 60.0f },
    { "Right Lower Leg Stretch", 4, 2, -80.0f, 80.0f },
    { "Right Lower Leg Twist In-Out", 4, 0, -90.0f, 90.0f },
    { "Right Foot Up-Down", 6, 2, -50.0f, 50.0f },
    { "Right Foot Twist In-Out", 6, 1, -30.0f, 30.0f },
    { "Right Toes Up-Down", 20, 2, -50.0f, 50.0f },
    { "Left Shoulder Down-Up", 11, 2, -15.0f, 30.0f },
    { "Left Shoulder Front-Back", 11, 1, -15.0f, 15.0f },
    { "Left Arm Down-Up", 13, 2, -60.0f, 100.0f },
    { "Left Arm Front-Back", 13, 1, -100.0f, 100.0f },
    { "Left Arm Twist In-Out", 13, 0, -90.0f, 90.0f },
    { "Left Forearm Stretch", 15, 2, -80.0f, 80.0f },
    { "Left Forearm Twist In-Out", 15, 0, -90.0f, 90.0f },
    { "Left Hand Down-Up", 17, 2, -80.0f, 80.0f },
    { "Left Hand In-Out", 17, 1, -40.0f, 40.0f },
    { "Right Shoulder Down-Up", 12, 2, -15.0f, 30.0f },
    { "Right Shoulder Front-Back", 12, 1, -15.0f, 15.0f },
    { "Right Arm Down-Up", 14, 2, -60.0f, 100.0f },
    { "Right Arm Front-Back", 14, 1, -100.0f, 100.0f },
    { "Right Arm Twist In-Out", 14, 0, -90.0f, 90.0f },
    { "Right Forearm Stretch", 16, 2, -80.0f, 80.0f },
    { "Right Forearm Twist In-Out", 16, 0, -90.0f, 90.0f },
    { "Right Hand Down-Up", 18, 2, -80.0f, 80.0f },
    { "Right Hand In-Out", 18, 1, -40.0f, 40.0f },
    { "Left Thumb 1 Stretched", 24, 2, -20.0f, 20.0f },
    { "Left Thumb Spread", 24, 1, -25.0f, 25.0f },
    { "Left Thumb 2 Stretched", 25, 2, -40.0f, 35.0f },
    { "Left Thumb 3 Stretched", 26, 2, -40.0f, 35.0f },
    { "Left Index 1 Stretched", 27, 2, -50.0f, 50.0f },
    { "Left Index Spread", 27, 1, -20.0f, 20.0f },
    { "Left Index 2 Stretched", 28, 2, -45.0f, 45.0f },
    { "Left Index 3 Stretched", 29, 2, -45.0f, 45.0f },
    { "Left Middle 1 Stretched", 30, 2, -50.0f, 50.0f },
    { "Left Middle Spread", 30, 1, -7.5f, 7.5f },
    { "Left Middle 2 Stretched", 31, 2, -45.0f, 45.0f },
    { "Left Middle 3 Stretched", 32, 2, -45.0f, 45.0f },
    { "Left Ring 1 Stretched", 33, 2, -50.0f, 50.0f },
    { "Left Ring Spread", 33, 1, -7.5f, 7.5f },
    { "Left Ring 2 Stretched", 34, 2, -45.0f, 45.0f },
    { "Left Ring 3 Stretched", 35, 2, -45.0f, 45.0f },
    { "Left Little 1 Stretched", 36, 2, -50.0f, 50.0f },
    { "Left Little Spread", 36, 1, -20.0f, 20.0f },
    { "Left Little 2 Stretched", 37, 2, -45.0f, 45.0f },
    { "Left Little 3 Stretched", 38, 2, -45.0f, 45.0f },
    { "Right Thumb 1 Stretched", 39, 2, -20.0f, 20.0f },
    { "Right Thumb Spread", 39, 1, -25.0f, 25.0f },
    { "Right Thumb 2 Stretched", 40, 2, -40.0f, 35.0f },
    { "Right Thumb 3 Stretched", 41, 2, -40.0f, 35.0f },
    { "Right Index 1 Stretched", 42, 2, -50.0f, 50.0f },
    { "Right Index Spread", 42, 1, -20.0f, 20.0f },
    { "Right Index 2 Stretched", 43, 2, -45.0f, 45.0f },
    { "Right Index 3 Stretched", 44, 2, -45.0f, 45.0f },
    { "Right Middle 1 Stretched", 45, 2, -50.0f, 50.0f },
    { "Right Middle Spread", 45, 1, -7.5f, 7.5f },
    { "Right Middle 2 Stretched", 46, 2, -45.0f, 45.0f },
    { "Right Middle 3 Stretched", 47, 2, -45.0f, 45.0f },
    { "Right Ring 1 Stretched", 48, 2, -50.0f, 50.0f },
    { "Right Ring Spread", 48, 1, -7.5f, 7.5f },
    { "Right Ring 2 Stretched", 49, 2, -45.0f, 45.0f },
    { "Right Ring 3 Stretched", 50, 2, -45.0f, 45.0f },
    { "Right Little 1 Stretched", 51, 2, -50.0f, 50.0f },
    { "Right Little Spread", 51, 1, -20.0f, 20.0f },
    { "Right Little 2 Stretched", 52, 2, -45.0f, 45.0f },
    { "Right Little 3 Stretched", 53, 2, -45.0f, 45.0f },
};
const int BONE_PARENTS[BoneCount] = { -1, 0, 0, 1, 2, 3, 4, 0, 7, 54, 9, 54, 54, 11, 12, 13, 14, 15, 16, 5, 6, 10, 10, 10, 17, 24, 25, 17, 27, 28, 17, 30, 31, 17, 33, 34, 17, 36, 37, 18, 39, 40, 18, 42, 43, 18, 45, 46, 18, 48, 49, 18, 51, 52, 8 };
const float LIMIT_SIGNS[BoneCount][3] = {
    { 1.0f, 1.0f, 1.0f },  // Hips
    { 1.0f, 1.0f, 1.0f },  // LeftUpperLeg
    { -1.0f, -1.0f, 1.0f },  // RightUpperLeg
    { 1.0f, 1.0f, -1.0f },  // LeftLowerLeg
    { -1.0f, 1.0f, -1.0f },  // RightLowerLeg
    { 1.0f, 1.0f, 1.0f },  // LeftFoot
    { 1.0f, -1.0f, 1.0f },  // RightFoot
    { 1.0f, 1.0f, 1.0f },  // Spine
    { 1.0f, 1.0f, 1.0f },  // Chest
    { 1.0f, 1.0f, 1.0f },  // Neck
    { 1.0f, 1.0f, 1.0f },  // Head
    { 1.0f, 1.0f, -1.0f },  // LeftShoulder
    { 1.0f, 1.0f, 1.0f },  // RightShoulder
    { 1.0f, 1.0f, -1.0f },  // LeftUpperArm
    { -1.0f, 1.0f, 1.0f },  // RightUpperArm
    { 1.0f, 1.0f, -1.0f },  // LeftLowerArm
    { -1.0f, 1.0f, 1.0f },  // RightLowerArm
    { 1.0f, 1.0f, -1.0f },  // LeftHand
    { 1.0f, 1.0f, 1.0f },  // RightHand
    { 1.0f, 1.0f, 1.0f },  // LeftToes
    { 1.0f, 1.0f, 1.0f },  // RightToes
    { 1.0f, 1.0f, -1.0f },  // LeftEye
    { 1.0f, -1.0f, -1.0f },  // RightEye
    { 1.0f, 1.0f, 1.0f },  // Jaw
    { -1.0f, -1.0f, 1.0f },  // Left Thumb Proximal
    { -1.0f, 1.0f, 1.0f },  // Left Thumb Intermediate
    { -1.0f, 1.0f, 1.0f },  // Left Thumb Distal
    { -1.0f, -1.0f, -1.0f },  // Left Index Proximal
    { -1.0f, 1.0f, -1.0f },  // Left Index Intermediate
    { -1.0f, 1.0f, -1.0f },  // Left Index Distal
    { -1.0f, -1.0f, -1.0f },  // Left Middle Proximal
    { -1.0f, 1.0f, -1.0f },  // Left Middle Intermediate
    { -1.0f, 1.0f, -1.0f },  // Left Middle Distal
    { -1.0f, 1.0f, -1.0f },  // Left Ring Proximal
    { -1.0f, 1.0f, -1.0f },  // Left Ring Intermediate
    { -1.0f, 1.0f, -1.0f },  // Left Ring Distal
    { -1.0f, 1.0f, -1.0f },  // Left Little Proximal
    { -1.0f, 1.0f, -1.0f },  // Left Little Intermediate
    { -1.0f, 1.0f, -1.0f },  // Left Little Distal
    { 1.0f, -1.0f, -1.0f },  // Right Thumb Proximal
    { 1.0f, 1.0f, -1.0f },  // Right Thumb Intermediate
    { 1.0f, 1.0f, -1.0f },  // Right Thumb Distal
    { 1.0f, -1.0f, 1.0f },  // Right Index Proximal
    { 1.0f, 1.0f, 1.0f },  // Right Index Intermediate
    { 1.0f, 1.0f, 1.0f },  // Right Index Distal
    { 1.0f, -1.0f, 1.0f },  // Right Middle Proximal
    { 1.0f, 1.0f, 1.0f },  // Right Middle Intermediate
    { 1.0f, 1.0f, 1.0f },  // Right Middle Distal
    { 1.0f, 1.0f, 1.0f },  // Right Ring Proximal
    { 1.0f, 1.0f, 1.0f },  // Right Ring Intermediate
    { 1.0f, 1.0f, 1.0f },  // Right Ring Distal
    { 1.0f, 1.0f, 1.0f },  // Right Little Proximal
    { 1.0f, 1.0f, 1.0f },  // Right Little Intermediate
    { 1.0f, 1.0f, 1.0f },  // Right Little Distal
    { 1.0f, 1.0f, 1.0f },  // UpperChest
};
// Muscle values of a T-pose (a test avatar's, as Unity reads the skeleton in its humanoid description).
const float TPOSE_MUSCLES[MUSCLE_COUNT] = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.600109f, 0.0f, 0.0f,
    0.999916f, 0.0f, 0.0f, 0.0f, 0.0f, 0.600109f, 0.0f, 0.0f,
    0.999916f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.400251f,
    0.30017f, 0.00518332f, 0.999908f, -0.0057969f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.400251f, 0.30017f, 0.00518172f, 0.999907f, -0.00579471f, 0.0f, 0.0f, -0.649033f,
    0.463406f, 0.64606f, 0.64606f, 0.669288f, -0.439275f, 0.811222f, 0.811222f, 0.668398f,
    -0.564854f, 0.811222f, 0.811222f, 0.666886f, -0.651693f, 0.811222f, 0.811222f, 0.666263f,
    -0.471696f, 0.811222f, 0.811222f, -0.649038f, 0.463409f, 0.64606f, 0.64606f, 0.669288f,
    -0.439275f, 0.811222f, 0.811222f, 0.668398f, -0.564855f, 0.811222f, 0.811222f, 0.666885f,
    -0.651694f, 0.811222f, 0.811222f, 0.666264f, -0.471697f, 0.811222f, 0.811222f,
};
