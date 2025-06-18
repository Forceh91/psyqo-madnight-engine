#include "vector.hh"

#include "psyqo/gte-registers.hh"
#include "psyqo/gte-kernels.hh"

psyqo::Vec3 GTEMathVector::ProjectOntoAxes(const psyqo::Matrix33 &axisMatrix, const psyqo::Vec3 &normalizedVec)
{
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(axisMatrix);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(normalizedVec);
    psyqo::GTE::Kernels::mvmva<psyqo::GTE::Kernels::MX::RT, psyqo::GTE::Kernels::MV::V0, psyqo::GTE::Kernels::TV::Zero>();
    return psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();
}
