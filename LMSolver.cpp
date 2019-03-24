#include <iostream>
#include <fstream>

#include "solver/GTSAMSolver.h"
#include "prototype/MyGTSAMSolver.h"

struct QuadraticEvaluationFunction {
    float operator()(const Eigen::VectorXf &params, const Eigen::VectorXf &x) const
    {
        float xf = x(0);
        float a = params(0);
        float b = params(1);
        float c = params(2);
        return a * xf * xf + b * xf + c;
    }
};

typedef float (*EvaluationFunction)(const Eigen::VectorXf, const Eigen::VectorXf);

template<unsigned int M, unsigned int N, typename EvaluationFunction>
class MyFunctor
{
    Eigen::MatrixXf measuredValues;
    const EvaluationFunction evalFunction;

public:
    MyFunctor(Eigen::MatrixXf measuredValues):
        measuredValues(measuredValues), evalFunction(EvaluationFunction())
    {}

    int operator()(const Eigen::VectorXf &params, Eigen::VectorXf &fvec) const
    {

        for (int i = 0; i < values(); i++) {
            float xValue = measuredValues(i, 0);
            float yValue = measuredValues(i, 1);

            Eigen::VectorXf xVector(1);
            xVector(0) = xValue;

            float residual = yValue - evalFunction(params, xVector);
            fvec(i) = residual;
        }
        return 0;
    }

    int df(const Eigen::VectorXf &x, Eigen::MatrixXf &fjacobian) {
        float epsilon = 1e-5f;

        for (int i = 0; i < x.size(); i++) {
            Eigen::VectorXf xPlus(x);
            Eigen::VectorXf xMinus(x);
            xPlus(i) += epsilon;
            xMinus(i) -= epsilon;

            Eigen::VectorXf fVecPlus(values());
            Eigen::VectorXf fVecMinus(values());

            operator()(xPlus, fVecPlus);
            operator()(xMinus, fVecMinus);

            Eigen::VectorXf fvecDiff(values());
            fvecDiff = (fVecPlus - fVecMinus) / (2 * epsilon);

            // Assign a block of size Mx1 starting at (0,i)
            // TODO(jeremy): Add support for vector functions
            fjacobian.block<M, 1>(0, i) = fvecDiff;
        }
        return 0;
    }
                int values() const { return M; }

    int inputs() const { return N; }
};

const int M = 100; // Number of measurements
const int N = 3; // Number of parameters: a, b, c
int main() {
    std::ifstream infile("measurements.txt");

    if (!infile) {
        std::cout << "measurements.txt could not be read" << std::endl;
        return -1;
    }

    std::vector<float> xValues;
    std::vector<float> yValues;

    std::string currLine;
    while(getline(infile, currLine)) {
        std::istringstream ss(currLine);
        float x, y;
        ss >> x >> y;
        xValues.push_back(x);
        yValues.push_back(y);
    }


    Eigen::MatrixXf measuredValues(M, 2);
    for (int i = 0; i < M; i++) {
        measuredValues(i, 0) = xValues[i];
        measuredValues(i, 1) = yValues[i];
    }

    #define EIGEN_NO_MALLOC = 1
    using ParamVector = Eigen::Matrix<float, -1, 1, 0, -1, 1>;
    ParamVector x;
    float mydata[N]; // No mallocs!
    mydata[0] = 0;
    mydata[1] = 1;
    mydata[2] = 2;
    float* data_ptr = mydata;
    x = Eigen::Map<ParamVector, Eigen::Unaligned>(data_ptr, N, 1);
    #undef EIGEN_NO_MALLOC

    using QuadraticFunctor = MyFunctor<M, N, QuadraticEvaluationFunction>;
    QuadraticFunctor functor(measuredValues);

    Eigen::LevenbergMarquardt<QuadraticFunctor, float> lm(functor);
    int result = lm.minimize(x);

    std::cout << "Opt result" << std::endl;
    std::cout << "\ta: " << x(0) << std::endl;
    std::cout << "\tb: " << x(1) << std::endl;
    std::cout << "\tc: " << x(2) << std::endl;
}