#include "gte-math.hh"

#include "psyqo/gte-registers.hh"
#include "psyqo/gte-kernels.hh"

psyqo::Vec3 GTEMath::ProjectVectorOntoAxes(const psyqo::Matrix33 &axisMatrix, const psyqo::Vec3 &normalizedVec)
{
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(axisMatrix);
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V0>(normalizedVec);
    psyqo::GTE::Kernels::mvmva<psyqo::GTE::Kernels::MX::RT, psyqo::GTE::Kernels::MV::V0, psyqo::GTE::Kernels::TV::Zero>();
    return psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();
}

void GTEMath::MultiplyMatrix33(const psyqo::Matrix33 &rotationMatrixA, const psyqo::Matrix33 rotationMatrixB, psyqo::Matrix33 *out)
{
    psyqo::Vec3 svOut = psyqo::Vec3::ZERO();

    // only need to write matrix A once
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::Rotation>(rotationMatrixA);

    // we need to do this 3 times for matrix B, x then y then z
    // this is x
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(psyqo::Vec3{rotationMatrixB.vs[0].x, rotationMatrixB.vs[1].x, rotationMatrixB.vs[2].x});

    // do the mvma which gets put into SV, then read it out from SV
    psyqo::GTE::Kernels::mvmva<psyqo::GTE::Kernels::MX::RT, psyqo::GTE::Kernels::MV::V1>();
    svOut = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();

    // update the out matrix
    out->vs[0].x = svOut.x;
    out->vs[1].x = svOut.y;
    out->vs[2].x = svOut.z;

    // this is y
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(psyqo::Vec3{rotationMatrixB.vs[0].y, rotationMatrixB.vs[1].y, rotationMatrixB.vs[2].y});

    // do the mvma which gets put into SV, then read it out from SV
    psyqo::GTE::Kernels::mvmva<psyqo::GTE::Kernels::MX::RT, psyqo::GTE::Kernels::MV::V1>();
    svOut = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();

    // update the out matrix
    out->vs[0].y = svOut.x;
    out->vs[1].y = svOut.y;
    out->vs[2].y = svOut.z;

    // this is z
    psyqo::GTE::writeSafe<psyqo::GTE::PseudoRegister::V1>(psyqo::Vec3{rotationMatrixB.vs[0].z, rotationMatrixB.vs[1].z, rotationMatrixB.vs[2].z});

    // do the mvma which gets put into SV, then read it out from SV
    psyqo::GTE::Kernels::mvmva<psyqo::GTE::Kernels::MX::RT, psyqo::GTE::Kernels::MV::V1>();
    svOut = psyqo::GTE::readSafe<psyqo::GTE::PseudoRegister::SV>();

    // update the out matrix
    out->vs[0].z = svOut.x;
    out->vs[1].z = svOut.y;
    out->vs[2].z = svOut.z;
}
