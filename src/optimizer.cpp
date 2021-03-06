#include"optimizer.h"

namespace DirectionUpdate {

  // steep descent update search direction
  SearchDirection sd_update(const Gradient &g) {
    return -1.0 * g;
  }

  // conjugate update search direction in one iteration
  SearchDirection cg_update(const Gradient &last_gradient, const SearchDirection &last_direction,
                            const Gradient &this_gradient)
  {
    double beta = dot(this_gradient, this_gradient) / dot(last_gradient, last_gradient);
    return -this_gradient + beta * last_direction;
  }

  // L-BFGS update search direction
  SearchDirection lbfgs_update(const vector<IterationPoint>& sks, const vector<Gradient>& yks, const Gradient &this_gradient) {

    int m = sks.size();

    cout << "this gradient: " << this_gradient << endl;
    SearchDirection q(this_gradient.begin(), this_gradient.end());
    // temporary coeff in the calculation
    vector<double> rhos(m), alphas(m);
    // left
    for(int i=m-1; i>-1; --i){
      cout << "yks i: " << yks[i] << endl;
      cout << "sks i: " << sks[i] << endl;
      cout << "yks i: " << norm(yks[i]) << "| sks i: " << norm(sks[i]) << endl;
      double rhoi = 1 / dot(yks[i], sks[i]);
      rhos[i] = rhoi;
      double ai = rhoi * dot(sks[i], q);
      alphas[i] = ai;
      cout << "rho: " << rhoi << " | alpha: " << ai << endl;
      q = q - ai * yks[i];
      cout << "q norm: " << norm(q) << endl;
    }

    // middle
    // diagonal Hessian0
    SearchDirection hess0(this_gradient.size(), 1.0);
    SearchDirection r = hess0 * q;
    cout << "r norm: " << norm(r) << endl;

    // right
    for(int i=0; i<m; ++i){
      double beta = rhos[i] * dot(yks[i], r);
      r = r + sks[i] * (alphas[i] - beta);
    }
    cout << "r norm 2: " << norm(r) << endl;

    return -r;
  }
}

namespace FuncOptimizer {

  double FunctionOptimizer::line_search(
      const IterationPoint &start_point, const SearchDirection &direc,
      IterationPoint &end_point, const int max_iter) {
    double v0 = func_wrapper.valueAt(start_point);
    bool find = false;
    int niter = 0;
    double dalpha = 0.0001;

    SearchDirection incre = direc * dalpha;
    IterationPoint p0(start_point), p1(end_point.size());
    while (niter < max_iter) {
      //cout << "line search iter: " << niter << endl;
      SearchDirection p1 = p0 + incre;
      //cout << p1 << endl;
      double v1 = func_wrapper.valueAt(p1);
      //cout << "v0 and v1: " << v0 << ", " << v1 << endl;
      if (v1 > v0) {
        find = true;
        break;
      }
      std::swap(p0, p1);
      std::swap(v0, v1);
      ++niter;
    }
    cout << "niter in line search: " << niter << endl;

    end_point = p0;
    double alpha = niter * dalpha;
    return alpha;
  }

  vector<IterationPoint> SteepDescent::optimize(
      const IterationPoint &start_point, const double threshold,
      const int max_iter, const bool verbose) {
    IterationPoint x0(start_point.begin(), start_point.end());
    SearchDirection g0;
    IterationPoint x1(start_point.size());
    vector<IterationPoint> paths;

    int niter = 0;
    bool converged = false;
    while (!converged && niter < max_iter) {
      paths.push_back(x0);
      g0 = func_wrapper.gradientAt(x0);
      if (norm(g0, 2) < threshold) {
        converged = true;
        break;
      }
      SearchDirection p0 = DirectionUpdate::sd_update(g0);

      //cout << "============> " << niter << endl;
      //cout << "x0:" << x0 << "| v0:" << func_wrapper.valueAt(x0) << endl;
      double alpha = line_search(x0, p0, x1);
      double v1 = func_wrapper.valueAt(x1);
      //cout << "alpha: " << alpha << endl;
      //cout << "x1:" << x1 << "| v1:" << func_wrapper.valueAt(x1) << endl;

      std::swap(x0, x1);
      ++niter;
    }

    return paths;
  }

  vector<IterationPoint> ConjugateGradient::optimize(
      const IterationPoint &start_point, const double threshold,
      const int max_iter, const bool verbose) {
    IterationPoint x0(start_point.begin(), start_point.end());
    SearchDirection g0 = func_wrapper.gradientAt(x0);
    IterationPoint x1(start_point.size());
    SearchDirection g1(start_point.size());
    // search direction
    SearchDirection p = -g0;
    vector<IterationPoint> paths;

    int niter = 0;
    bool converged = false;
    bool restart;
    while (!converged && niter < max_iter) {
      paths.push_back(x0);
      if (norm(g0, 2) < threshold) {
        converged = true;
        break;
      }

      double alpha = line_search(x0, p, x1);
      g1 = func_wrapper.gradientAt(x1);
      double orth = dot(g0, g1) / dot(g1, g1);

      if (abs(orth) > 0.1) {
        restart = true;
        p = DirectionUpdate::sd_update(g1);
      } else {
        restart = false;
        p = DirectionUpdate::cg_update(g0, p, g1);
      }

      cout << "[" << niter << "] x: " << x0 << " | v: " << func_wrapper.valueAt(x0)
           << " | norm: " << norm(g1) << " | orth: " << orth << " | restart: " << restart << endl;

      g0 = g1;
      x0 = x1;
      ++niter;
    }

    return paths;
  }


  vector<IterationPoint> LBFGS::optimize(
      const IterationPoint &start_point, const double threshold,
      const int max_iter, const bool verbose, int memory)
  {
    IterationPoint x0 = start_point;
    Gradient g0 = func_wrapper.gradientAt(x0);
    IterationPoint x1(start_point.size());
    Gradient g1(start_point.size());
    vector<IterationPoint> paths;

    int niter = 0;
    bool converged = false;
    SearchDirection p;
    vector<IterationPoint> sks;
    vector<Gradient > yks;
    while(!converged && niter < max_iter){
      cout << "========================>\n [" << niter << "]" << endl;
      paths.push_back(x0);
      if(norm(g0, 2) < threshold){
        converged = true;
        break;
      }

      if(niter == 0){
        p = DirectionUpdate::sd_update(g0);
      } else {
        p = DirectionUpdate::lbfgs_update(sks, yks, g0);
      }

      double alpha = line_search(x0, p, x1);

      cout << "---> summary:" << endl;
      cout << "x0: " << x0 << " | v0: " << func_wrapper.valueAt(x0) << endl;
      cout << "x1: " << x1 << " | v1: " << func_wrapper.valueAt(x1) << endl;
      cout << "diff norm" << norm(x1 - x0) << endl;
      cout << "norm:" << norm(g0) << endl;

      // clean the sks and yks which are m iterations before
      if(niter >= memory){
        yks.erase(yks.begin());
        sks.erase(sks.begin());
      }
      sks.push_back(x1 - x0);
      g1 = func_wrapper.gradientAt(x1);
      yks.push_back(g1 - g0);

      x0 = x1;
      g0 = g1;
      ++niter;
    }

    return paths;

  }
} // end namespace
