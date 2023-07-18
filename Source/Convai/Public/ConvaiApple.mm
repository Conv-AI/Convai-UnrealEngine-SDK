#ifdef __APPLE__
#import <AVFoundation/AVFoundation.h>

bool GetAppleMicPermission()
{
    __block bool permissionGranted = false;

    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio])
    {
        case AVAuthorizationStatusAuthorized:
            // Already authorized
            permissionGranted = true;
            break;
        case AVAuthorizationStatusNotDetermined:
        {
            dispatch_semaphore_t sem = dispatch_semaphore_create(0);
            // The user has not yet been asked to grant access to the audio capture device.
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted)
            {
                if (granted)
                {
                    // Permission granted
                    permissionGranted = true;
                }
                dispatch_semaphore_signal(sem);
            }];
            dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
            break;
        }
        case AVAuthorizationStatusRestricted:
        case AVAuthorizationStatusDenied:
            // The user has previously denied access or can't grant access.
            permissionGranted = false;
            break;
    }

    return permissionGranted;
}
#endif
