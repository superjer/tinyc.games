        // dumb rand test
        #define RUNS 1000000
        int seed = time(NULL);
        int RANDmin      =  999999999;
        int RANDmax      = -999999999;
        double RAND01min =  999999999;
        double RAND01max = -999999999;
        int RANDImin     =  999999999;
        int RANDImax     = -999999999;
        double RANDFmin  =  999999999;
        double RANDFmax  = -999999999;
        int RANDPsum     = 0;
        for (int i = 0; i < RUNS; i++)
        {
                int r = RAND;
                if (r < RANDmin) RANDmin = r;
                if (r > RANDmax) RANDmax = r;
                //printf("RAND is %d\n", r);
        }
        for (int i = 0; i < RUNS; i++)
        {
                double r = RAND01;
                if (r < RAND01min) RAND01min = r;
                if (r > RAND01max) RAND01max = r;
        }
        for (int i = 0; i < RUNS; i++)
        {
                double r = RANDI(-50, 50);
                if (r < RANDImin) RANDImin = r;
                if (r > RANDImax) RANDImax = r;
        }
        for (int i = 0; i < RUNS; i++)
        {
                double r = RANDF(-1.f, 1.f);
                if (r < RANDFmin) RANDFmin = r;
                if (r > RANDFmax) RANDFmax = r;
        }
        for (int i = 0; i < RUNS; i++)
        {
                RANDPsum += RANDP(50);
        }
        printf("RAND min %d, max %d\n", RANDmin, RANDmax);
        printf("RAND01 min %f, max %f\n", RAND01min, RAND01max);
        printf("RANDI min %d, max %d\n", RANDImin, RANDImax);
        printf("RANDF min %f, max %f\n", RANDFmin, RANDFmax);
        printf("RANDP %f\n", RANDPsum / (double)RUNS * 100.f);
        exit(-1);
