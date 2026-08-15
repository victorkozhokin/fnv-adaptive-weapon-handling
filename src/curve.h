#pragma once

namespace curve {

struct Node {
	float pitch;   // degrees
	float value;
};

// Linear interpolation through `nodes`, which must be ascending by pitch.
// Clamps to the end values outside the range.
float Interpolate(const Node *nodes, unsigned count, float pitchDegrees);

} // namespace curve
