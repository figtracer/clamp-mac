// Runs the plugin's real audio path (Clamp/ClampDSP.h) over a script on stdin.
//
// Input lines:
//   params <bypass> <oversampling> <inDB> <outDB> <ceilingDB> <shape> <type> <mode>
//   osreset   (what OnParamChange does for OVERSAMPLING)
//   block <n> <l0> <r0> <l1> <r1> ...
// Each block prints one line of interleaved output samples.

#include "../../Clamp/ClampDSP.h"

#include <cstdio>
#include <cstring>
#include <vector>

int main()
{
  ClampDSP dsp;
  dsp.SetBlockSize(512);
  ClampDSP::Params p;
  char cmd[16];
  while (scanf("%15s", cmd) == 1)
  {
    if (!strcmp(cmd, "params"))
    {
      int bypass;
      scanf("%d %d %lf %lf %lf %lf %d %d", &bypass, &p.oversampling, &p.inDB, &p.outDB, &p.ceilingDB, &p.shape, &p.type, &p.mode);
      p.bypass = bypass != 0;
    }
    else if (!strcmp(cmd, "osreset"))
    {
      dsp.RequestOversamplingReset();
    }
    else if (!strcmp(cmd, "block"))
    {
      int n;
      scanf("%d", &n);
      std::vector<double> l(n), r(n), ol(n), orr(n);
      for (int i = 0; i < n; i++)
        scanf("%lf %lf", &l[i], &r[i]);
      double* ins[2] = {l.data(), r.data()};
      double* outs[2] = {ol.data(), orr.data()};
      dsp.Process(ins, outs, n, p);
      for (int i = 0; i < n; i++)
        printf("%a %a ", ol[i], orr[i]);
      printf("\n");
    }
  }
}
