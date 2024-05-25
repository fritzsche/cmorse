#include <math.h>

double blackman_harris_kernel(double x) {
  const double a0 = 0.35875; 
  const double a1 = 0.48829;	
  const double a2 = 0.14128;	
  const double a3 = 0.01168; 
  return a0 - a1*cos(2*M_PI*x) + a2*cos(4*M_PI*x) - a3*cos(6*M_PI*x);      
} 

void backman_harris_step_response(double *pOutput, const int len) {
  for (int i = 0; i< len;i++) pOutput[i] = blackman_harris_kernel((double)i/len);
  for (int i = 0; i< len;i++) pOutput[i] = pOutput[i-1] + pOutput[i]; 
  double scale = (double)1 / pOutput[len - 1];   
  for (int i = 0; i< len;i++) pOutput[i] = pOutput[i] * scale; 
}
