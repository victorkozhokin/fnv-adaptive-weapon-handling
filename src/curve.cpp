#include "curve.h"

namespace curve {

float Interpolate(const Node *nodes, unsigned count, float pitch)
{
	if (count == 0)
		return 0.f;

	if (pitch <= nodes[0].pitch)
		return nodes[0].value;

	for (unsigned i = 1; i < count; i++) {
		if (pitch > nodes[i].pitch)
			continue;

		const auto &a = nodes[i - 1];
		const auto &b = nodes[i];
		const auto span = b.pitch - a.pitch;

		if (span <= 0.f)
			return b.value;

		return a.value + (b.value - a.value) * ((pitch - a.pitch) / span);
	}

	return nodes[count - 1].value;
}

} // namespace curve
