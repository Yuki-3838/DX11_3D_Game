#include <SimpleMath.h>

namespace DirectX::SimpleMath
{
const Matrix Matrix::Identity = Matrix(
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f);

const Quaternion Quaternion::Identity = Quaternion(0, 0, 0, 1);
}
