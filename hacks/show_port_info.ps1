Set-StrictMode -Version Latest

# From a Windows command prompt:
#     powershell.exe -NoLogo -NoProfile -ExecutionPolicy RemoteSigned -Command ". .\show_port_info.ps1; Get-BusPirateDetails"

# From a Powershell command prompt:
#     # First, load the script from a Windows Powershell prompt:
#     . .\show_port_info.ps1
#     # If necessary, permit the loading of the script (only after reviewing what the script does below):
#     Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope Process
#     # Then, run the function that was defined by the script:
#     Get-BusPirateDetails

# If something goes awry:
#     Execute the script with more verbose output, which you can
#     then copy/paste into a GitHub issue:
#
#     Get-BusPirateDetails -Verbose -Debug

# Example output with a single BusPirate5 device connected:
#
# PS C:\Users\ian> Get-BusPirateDetails
# InstanceId          : USB\VID_1209&PID_7331\FEDCBA9876543210
# Serial              : FEDCBA9876543210
# Location            : Port_#0003.Hub_#0007
# InstanceID_Terminal : USB\VID_1209&PID_7331&MI_00\8&32E00187&0&0000
# Port_Terminal       : COM55
# InstanceID_Binary   : USB\VID_1209&PID_7331&MI_02\8&32E00187&0&0002
# Port_Binary         : COM56
# InstanceId_Storage  : USB\VID_1209&PID_7331&MI_04\8&32E00187&0&0004
#
# Which shows that, to connect to the terminal of this BP5, one should use COM55.
# similarly, the binary serial port is using COM56.
#

function Has-Property {
    param (
        [object]$Object,
        [string]$PropertyName
    )

    # Not all objects have the 'PSObject' property nor the 'Properties' property
    # So the following commented-out code is not great with strict mode enabled.    
    # if ($Object.PSObject.Properties.Match($PropertyName) -eq $null) {
    #     return $true;
    # } else {
    #     return $false;
    # }

    # Instead, brute force it (not great performance-wise, but it works)
    if ($Object -eq $null) {
        return $false;
    }
    $properties = $Object | Get-Member -MemberType Properties
    if ($properties.Name -contains $PropertyName) {
        return $true;
    } else {
        return $false;
    }

}
function Get-BusPirate {
    # USB VID and PID for the Bus Pirate devices seems to fluctuate slightly?
    #   0x1209 / 0x7331 - Bus Pirate 5
    #   0x1209 / 0x7332 - Bus Pirate 5
    #
    # The composite device instance ID is of the form:
    #   USB\VID_1209&PID_7331\{serial}
    #
    # The interface instance IDs are of the form:
    #   USB\VID_1209&PID_7331&MI{interface}\{USB_Specific_Path}
    # 
    # Get only the composite (root) device (no `MI{interface}`)
    #
    Get-PnpDevice -PresentOnly |
        Where-Object {
            $_.InstanceId -match '^USB\\VID_1209&PID_733[\dA-Fa-f]\\'
        }
}

function Get-BusPirateDetails {
    [CmdletBinding()]
    param (
        [Parameter(Mandatory=$false)]
        [object]$InstanceId,
        [switch]$IncludeResultsWithErrors
    )

    Write-Verbose "Function Entry - Get-BusPirateDetails"

    [System.Collections.Generic.List[String]]$errors = [System.Collections.Generic.List[String]]::new();

    [CimInstance]$rootDevice = $null;

    # get all instances, if no instance ID was provided
    if (-not $PSCmdlet.MyInvocation.BoundParameters.ContainsKey('InstanceId')) {
        Get-BusPirate |% {
            Get-BusPirateDetails -InstanceId $_;
        }
        return;
    } elseif ($InstanceId -is [CimInstance]) {
        $rootDevice = $InstanceId;
    } elseif ($InstanceId -is [String]) {
        $rootDevice = Get-PnpDevice -PresentOnly -InstanceId $InstanceId;
    } else {
        Out-Error "Unsupported input type: $($InstanceId.GetType().FullName) -- Only support [String] and [CimInstance]";
        return;        
    }

    Write-Verbose "Processing root device $($rootDevice.InstanceId)"

    $resultObj = New-Object PSObject -Property @{ InstanceId = $rootDevice.InstanceId };

    # Hack ... the serial number is the tail end of the instance ID
    $serialNumber = ($rootDevice.InstanceId -split '\\')[-1];
    Write-Verbose "Serial number: $($serialNumber)"
    $resultObj | Add-Member -MemberType NoteProperty -Name Serial -Value $serialNumber;

    # The USB hub and port data is sometimes useful
    $location = (Get-PnpDeviceProperty -InstanceId $rootDevice.InstanceId -KeyName DEVPKEY_Device_LocationInfo).Data;
    Write-Verbose "Location: $($location)"
    $resultObj | Add-Member -MemberType NoteProperty -Name Location -Value $location


    # Each composite device has a list of child devices, one for each interface.
    [String[]]$interfaces = (Get-PnpDeviceProperty -InstanceId $rootDevice.InstanceId -KeyName DEVPKEY_Device_BusRelations).Data
    Write-Verbose "Found $($interfaces.Count) interfaces"
    $interfaces |% { Write-Verbose "Interface: $_" }

    # Loop through those interfaces to add additional information to the result object
    $interfaces |% {

        Write-Verbose "Processing interface $_"

        [CimInstance] $interface = Get-PnpDevice -PresentOnly -InstanceId $_;

        Write-Verbose "    FriendlyName: $interface.FriendlyName"


        # Mass storage devices (MSC) have a friendly name of the form "USB Mass Storage Device"
        # Serial ports (CDC) have a friendly name of the form "USB Serial Device (COM<n>)" (where <n> is a decimal number)
        if ($interface.FriendlyName -match 'USB Mass Storage Device') {

            # add the mass storage instanceID to the result object
            if (Has-Property -PSObject $resultObj -PropertyName 'InstanceId_Storage') {
                $errors.Add("Multiple storage interfaces detected: $($interface.InstanceId) friendlyName $($interface.FriendlyName)");
            } else {
                $resultObj | Add-Member -MemberType NoteProperty -Name InstanceId_Storage -Value $interface.InstanceId;
            }

        } elseif ($interface.FriendlyName -match 'USB Serial Device \(COM(\d+)\)') {

            $port = "COM$($Matches[1])";

            # The actual type of the CDC interface is defined by the bus-reported device description
            # Currently, two CDC interfaces are supported, reporting the name as either:
            #   Bus Pirate BIN
            #   Bus Pirate CDC
            $interfaceName = (Get-PnpDeviceProperty -InstanceId $interface.InstanceId -KeyName DEVPKEY_Device_BusReportedDeviceDesc).Data;

            if ($interfaceName -match 'CDC$') {

                if (      Has-Property -PSObject $resultObj -PropertyName 'InstanceId_Terminal') {
                    $errors.Add("Multiple CDC interfaces (a) detected: $($interface.InstanceId) / $($interface.FriendlyName) / $($interfaceName)");
                } elseif (Has-Property -PSObject $resultObj -PropertyName 'Port_Terminal') {
                    $errors.Add("Multiple CDC interfaces (b) detected: $($interface.InstanceId) / $($interface.FriendlyName) / $($interfaceName)");
                } else {
                    $resultObj | Add-Member -MemberType NoteProperty -Name InstanceID_Terminal -Value $interface.InstanceId;
                    $resultObj | Add-Member -MemberType NoteProperty -Name Port_Terminal -Value $port;
                }

            } elseif ($interfaceName -match 'BIN') {

                if (      Has-Property -PSObject $resultObj -PropertyName 'InstanceId_Binary') {
                    $errors.Add("Multiple BIN interfaces (a) detected: $($interface.InstanceId) / $($interface.FriendlyName) / $($interfaceName)");
                } elseif (Has-Property -PSObject $resultObj -PropertyName 'Port_Binary') {
                    $errors.Add("Multiple BIN interfaces (b) detected: $($interface.InstanceId) / $($interface.FriendlyName) / $($interfaceName)");
                } else {
                    $resultObj | Add-Member -MemberType NoteProperty -Name InstanceID_Binary -Value $interface.InstanceId;
                    $resultObj | Add-Member -MemberType NoteProperty -Name Port_Binary -Value $port;
                }

            } else {

                $errors.Add("Unknown CDC interface: $($interface.InstanceId) / $($interface.FriendlyName) / $($interfaceName)");

            }

        } else {

            $errors.Add("Unknown interface type: $($interface.InstanceId) / $($interface.FriendlyName)");

        }

    }

    # Verify all the expected information was added.
    [String[]]$MandatoryProperties = @('InstanceId', 'Serial', 'Location', 'InstanceId_Terminal', 'Port_Terminal', 'InstanceId_Binary', 'Port_Binary', 'InstanceId_Storage');
    $MandatoryProperties |% {
        Write-Verbose "Checking for property: $_"
        [String]$propertyToCheck = $_;
        if (-not (Has-Property -Object $resultObj -PropertyName $propertyToCheck)) {
            Write-Verbose "Did not find property: $_"
            $errors.Add("Missing property: $($propertyToCheck)");
        }
    }

    # $errors.Add("Test error");

    # Were there any errors detected?
    if ($errors.Count -eq 0) {
        return $resultObj;
    } elseif ($IncludeResultsWithErrors) {
        $resultObj | Add-Member -MemberType NoteProperty -Name Errors -Value $errors;
        return $resultObj;
    }
    # else had errors, but flag to return objects with errors was not provided.
    Write-Error "Detected $($errors.Count) errors for device $($rootDevice.InstanceId)";
    $errors |% {
        Write-Debug "    Error: $($_)";
    }
    return;
}

