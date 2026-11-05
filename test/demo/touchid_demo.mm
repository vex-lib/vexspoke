#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication.h>

// Runs a native LocalAuthentication request and reports its completion.
int main(int argc, const char * argv[]) {
    (void)argc;
    (void)argv;
    @autoreleasepool {
        LAContext *ctx = [[LAContext alloc] init];
        NSString *localizedReason = @"Authenticate to confirm action";
        NSError *error = nil;

        LAPolicy policy = LAPolicyDeviceOwnerAuthenticationWithBiometrics;
        if (![ctx canEvaluatePolicy:policy error:&error]) {
            policy = LAPolicyDeviceOwnerAuthentication;
            if (![ctx canEvaluatePolicy:policy error:&error]) {
                NSLog(@"TouchID / Biometrics not available on this device: %@", error.localizedDescription);
                return 1;
            }
        }

        NSLog(@"TouchID available – presenting sheet. Use your finger/face.");
        dispatch_semaphore_t sema = dispatch_semaphore_create(0);
        __block BOOL authSuccess = NO;

        [ctx evaluatePolicy:policy
            localizedReason:localizedReason
                      reply:^(BOOL success, NSError * _Nullable err) {
            authSuccess = success;
            if (!success && err) {
                NSLog(@"Authentication error: %@", err.localizedDescription);
            }
            dispatch_semaphore_signal(sema);
        }];

        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
        NSLog(@"Auth %@", authSuccess ? @"SUCCEEDED" : @"FAILED/CANCELLED");
        return authSuccess ? 0 : 1;
    }
}

