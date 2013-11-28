/*
 * PROJECT:        ReactOS Floppy Disk Controller Driver
 * LICENSE:        GNU GPLv2 only as published by the Free Software Foundation
 * FILE:           drivers/storage/fdc/fdc/fdo.c
 * PURPOSE:        Functional Device Object routines
 * PROGRAMMERS:    Eric Kohl
 */

/* INCLUDES *******************************************************************/

#include "fdc.h"

/* FUNCTIONS ******************************************************************/

static IO_COMPLETION_ROUTINE ForwardIrpAndWaitCompletion;

static
NTSTATUS
NTAPI
ForwardIrpAndWaitCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context)
{
    if (Irp->PendingReturned)
        KeSetEvent((PKEVENT)Context, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}


NTSTATUS
ForwardIrpAndWait(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    PDEVICE_OBJECT LowerDevice = ((PFDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerDevice;
    KEVENT Event;
    NTSTATUS Status;

    ASSERT(LowerDevice);

    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    IoCopyCurrentIrpStackLocationToNext(Irp);

    DPRINT("Calling lower device %p\n", LowerDevice);
    IoSetCompletionRoutine(Irp, ForwardIrpAndWaitCompletion, &Event, TRUE, TRUE, TRUE);

    Status = IoCallDriver(LowerDevice, Irp);
    if (Status == STATUS_PENDING)
    {
        Status = KeWaitForSingleObject(&Event, Suspended, KernelMode, FALSE, NULL);
        if (NT_SUCCESS(Status))
            Status = Irp->IoStatus.Status;
    }

    return Status;
}


NTSTATUS
NTAPI
ForwardIrpAndForget(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    PDEVICE_OBJECT LowerDevice = ((PFDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension)->LowerDevice;

    ASSERT(LowerDevice);

    IoSkipCurrentIrpStackLocation(Irp);
    return IoCallDriver(LowerDevice, Irp);
}


NTSTATUS
NTAPI
FdcAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT Pdo)
{
    PFDO_DEVICE_EXTENSION DeviceExtension = NULL;
    PDEVICE_OBJECT Fdo = NULL;
    NTSTATUS Status;

    DPRINT1("FdcAddDevice()\n");

    ASSERT(DriverObject);
    ASSERT(Pdo);

    /* Create functional device object */
    Status = IoCreateDevice(DriverObject,
                            sizeof(FDO_DEVICE_EXTENSION),
                            NULL,
                            FILE_DEVICE_CONTROLLER,
                            FILE_DEVICE_SECURE_OPEN,
                            FALSE,
                            &Fdo);
    if (NT_SUCCESS(Status))
    {
        DeviceExtension = (PFDO_DEVICE_EXTENSION)Fdo->DeviceExtension;
        RtlZeroMemory(DeviceExtension, sizeof(FDO_DEVICE_EXTENSION));

        DeviceExtension->Common.IsFDO = TRUE;

        DeviceExtension->Fdo = Fdo;
        DeviceExtension->Pdo = Pdo;


        Status = IoAttachDeviceToDeviceStackSafe(Fdo, Pdo, &DeviceExtension->LowerDevice);
        if (!NT_SUCCESS(Status))
        {
            DPRINT1("IoAttachDeviceToDeviceStackSafe() failed with status 0x%08lx\n", Status);
            IoDeleteDevice(Fdo);
            return Status;
        }


        Fdo->Flags |= DO_DIRECT_IO;
        Fdo->Flags |= DO_POWER_PAGABLE;

        Fdo->Flags &= ~DO_DEVICE_INITIALIZING;
    }

    return Status;
}


static
NTSTATUS
FdcFdoStartDevice(
    IN PDEVICE_OBJECT DeviceObject,
    IN PCM_RESOURCE_LIST ResourceList,
    IN PCM_RESOURCE_LIST ResourceListTranslated)
{
    PFDO_DEVICE_EXTENSION DeviceExtension;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptor;
//    PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptorTranslated;
    ULONG i;

    DPRINT1("FdcFdoStartDevice called\n");

    DeviceExtension = (PFDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    ASSERT(DeviceExtension);

    if (ResourceList == NULL ||
        ResourceListTranslated == NULL)
    {
        DPRINT1("No allocated resources sent to driver\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (ResourceList->Count != 1)
    {
        DPRINT1("Wrong number of allocated resources sent to driver\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (ResourceList->List[0].PartialResourceList.Version != 1 ||
        ResourceList->List[0].PartialResourceList.Revision != 1 ||
        ResourceListTranslated->List[0].PartialResourceList.Version != 1 ||
        ResourceListTranslated->List[0].PartialResourceList.Revision != 1)
    {
        DPRINT1("Revision mismatch: %u.%u != 1.1 or %u.%u != 1.1\n",
                ResourceList->List[0].PartialResourceList.Version,
                ResourceList->List[0].PartialResourceList.Revision,
                ResourceListTranslated->List[0].PartialResourceList.Version,
                ResourceListTranslated->List[0].PartialResourceList.Revision);
        return STATUS_REVISION_MISMATCH;
    }

    for (i = 0; i < ResourceList->List[0].PartialResourceList.Count; i++)
    {
        PartialDescriptor = &ResourceList->List[0].PartialResourceList.PartialDescriptors[i];
//        PartialDescriptorTranslated = &ResourceListTranslated->List[0].PartialResourceList.PartialDescriptors[i];

        switch (PartialDescriptor->Type)
        {
            case CmResourceTypePort:
                DPRINT1("Port: 0x%lx (%lu)\n",
                        PartialDescriptor->u.Port.Start.u.LowPart,
                        PartialDescriptor->u.Port.Length);
                if (PartialDescriptor->u.Port.Length >= 6)
                    DeviceExtension->ControllerInfo.BaseAddress = (PUCHAR)PartialDescriptor->u.Port.Start.u.LowPart;
                break;

            case CmResourceTypeInterrupt:
                DPRINT1("Interrupt: Level %lu  Vector %lu\n",
                        PartialDescriptor->u.Interrupt.Level,
                        PartialDescriptor->u.Interrupt.Vector);
/*
                Dirql = (KIRQL)PartialDescriptorTranslated->u.Interrupt.Level;
                Vector = PartialDescriptorTranslated->u.Interrupt.Vector;
                Affinity = PartialDescriptorTranslated->u.Interrupt.Affinity;
                if (PartialDescriptorTranslated->Flags & CM_RESOURCE_INTERRUPT_LATCHED)
                    InterruptMode = Latched;
                else
                    InterruptMode = LevelSensitive;
                ShareInterrupt = (PartialDescriptorTranslated->ShareDisposition == CmResourceShareShared);
*/
                break;

            case CmResourceTypeDma:
                DPRINT1("Dma: Channel %lu\n",
                        PartialDescriptor->u.Dma.Channel);
                break;
        }
    }

    return STATUS_SUCCESS;
}


static
NTSTATUS
NTAPI
FdcFdoConfigCallback(
    PVOID Context,
    PUNICODE_STRING PathName,
    INTERFACE_TYPE BusType,
    ULONG BusNumber,
    PKEY_VALUE_FULL_INFORMATION *BusInformation,
    CONFIGURATION_TYPE ControllerType,
    ULONG ControllerNumber,
    PKEY_VALUE_FULL_INFORMATION *ControllerInformation,
    CONFIGURATION_TYPE PeripheralType,
    ULONG PeripheralNumber,
    PKEY_VALUE_FULL_INFORMATION *PeripheralInformation)
{
    PKEY_VALUE_FULL_INFORMATION ControllerFullDescriptor;
    PCM_FULL_RESOURCE_DESCRIPTOR ControllerResourceDescriptor;
    PKEY_VALUE_FULL_INFORMATION PeripheralFullDescriptor;
    PCM_FULL_RESOURCE_DESCRIPTOR PeripheralResourceDescriptor;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptor;
    PCM_FLOPPY_DEVICE_DATA FloppyDeviceData;
    PFDO_DEVICE_EXTENSION DeviceExtension;
    PDRIVE_INFO DriveInfo;
    BOOLEAN ControllerFound = FALSE;
    ULONG i;

    DPRINT1("FdcFdoConfigCallback() called\n");

    DeviceExtension = (PFDO_DEVICE_EXTENSION)Context;

    /* Get the controller resources */
    ControllerFullDescriptor = ControllerInformation[IoQueryDeviceConfigurationData];
    ControllerResourceDescriptor = (PCM_FULL_RESOURCE_DESCRIPTOR)((PCHAR)ControllerFullDescriptor +
                                                                  ControllerFullDescriptor->DataOffset);

    for(i = 0; i < ControllerResourceDescriptor->PartialResourceList.Count; i++)
    {
        PartialDescriptor = &ControllerResourceDescriptor->PartialResourceList.PartialDescriptors[i];

        if (PartialDescriptor->Type == CmResourceTypePort)
        {
            if ((PUCHAR)PartialDescriptor->u.Port.Start.LowPart == DeviceExtension->ControllerInfo.BaseAddress)
                ControllerFound = TRUE;
        }
    }

    /* Leave, if the enumerated controller is not the one represented by the FDO */
    if (ControllerFound == FALSE)
        return STATUS_SUCCESS;

    /* Get the peripheral resources */
    PeripheralFullDescriptor = PeripheralInformation[IoQueryDeviceConfigurationData];
    PeripheralResourceDescriptor = (PCM_FULL_RESOURCE_DESCRIPTOR)((PCHAR)PeripheralFullDescriptor +
                                                                  PeripheralFullDescriptor->DataOffset);

    DeviceExtension->ControllerInfo.NumberOfDrives = 0;

    /* learn about drives attached to controller */
    for(i = 0; i < PeripheralResourceDescriptor->PartialResourceList.Count; i++)
    {
        PartialDescriptor = &PeripheralResourceDescriptor->PartialResourceList.PartialDescriptors[i];

        if (PartialDescriptor->Type != CmResourceTypeDeviceSpecific)
            continue;

        FloppyDeviceData = (PCM_FLOPPY_DEVICE_DATA)(PartialDescriptor + 1);

        DriveInfo = &DeviceExtension->ControllerInfo.DriveInfo[i];

        DriveInfo->ControllerInfo = &DeviceExtension->ControllerInfo;
        DriveInfo->UnitNumber = i;

        DriveInfo->FloppyDeviceData.MaxDensity = FloppyDeviceData->MaxDensity;
        DriveInfo->FloppyDeviceData.MountDensity = FloppyDeviceData->MountDensity;
        DriveInfo->FloppyDeviceData.StepRateHeadUnloadTime = FloppyDeviceData->StepRateHeadUnloadTime;
        DriveInfo->FloppyDeviceData.HeadLoadTime = FloppyDeviceData->HeadLoadTime;
        DriveInfo->FloppyDeviceData.MotorOffTime = FloppyDeviceData->MotorOffTime;
        DriveInfo->FloppyDeviceData.SectorLengthCode = FloppyDeviceData->SectorLengthCode;
        DriveInfo->FloppyDeviceData.SectorPerTrack = FloppyDeviceData->SectorPerTrack;
        DriveInfo->FloppyDeviceData.ReadWriteGapLength = FloppyDeviceData->ReadWriteGapLength;
        DriveInfo->FloppyDeviceData.FormatGapLength = FloppyDeviceData->FormatGapLength;
        DriveInfo->FloppyDeviceData.FormatFillCharacter = FloppyDeviceData->FormatFillCharacter;
        DriveInfo->FloppyDeviceData.HeadSettleTime = FloppyDeviceData->HeadSettleTime;
        DriveInfo->FloppyDeviceData.MotorSettleTime = FloppyDeviceData->MotorSettleTime;
        DriveInfo->FloppyDeviceData.MaximumTrackValue = FloppyDeviceData->MaximumTrackValue;
        DriveInfo->FloppyDeviceData.DataTransferLength = FloppyDeviceData->DataTransferLength;

        /* Once it's all set up, acknowledge its existance in the controller info object */
        DeviceExtension->ControllerInfo.NumberOfDrives++;
    }

    DeviceExtension->ControllerInfo.Populated = TRUE;

    DPRINT1("Detected %lu floppy drives!\n",
            DeviceExtension->ControllerInfo.NumberOfDrives);

    return STATUS_SUCCESS;
}


static
NTSTATUS
FdcFdoQueryBusRelations(
    IN PDEVICE_OBJECT DeviceObject,
    OUT PDEVICE_RELATIONS *DeviceRelations)
{
    PFDO_DEVICE_EXTENSION DeviceExtension;
    INTERFACE_TYPE InterfaceType = Isa;
    CONFIGURATION_TYPE ControllerType = DiskController;
    CONFIGURATION_TYPE PeripheralType = FloppyDiskPeripheral;
    NTSTATUS Status;

    DPRINT1("FdcFdoQueryBusRelations() called\n");

    DeviceExtension = (PFDO_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    Status = IoQueryDeviceDescription(&InterfaceType,
                                      NULL,
                                      &ControllerType,
                                      NULL,
                                      &PeripheralType,
                                      NULL,
                                      FdcFdoConfigCallback,
                                      DeviceExtension);
    if (!NT_SUCCESS(Status) && (Status != STATUS_OBJECT_NAME_NOT_FOUND))
    {
        return Status;
    }

    return STATUS_SUCCESS;
}


NTSTATUS
NTAPI
FdcFdoPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp)
{
    PIO_STACK_LOCATION IrpSp;
    PDEVICE_RELATIONS DeviceRelations = NULL;
    ULONG_PTR Information = 0;
    NTSTATUS Status = STATUS_NOT_SUPPORTED;

    DPRINT1("FdcFdoPnp()\n");

    IrpSp = IoGetCurrentIrpStackLocation(Irp);

    switch (IrpSp->MinorFunction)
    {
        case IRP_MN_START_DEVICE:
            DPRINT1("  IRP_MN_START_DEVICE received\n");
            /* Call lower driver */
            Status = ForwardIrpAndWait(DeviceObject, Irp);
            if (NT_SUCCESS(Status))
            {
                Status = FdcFdoStartDevice(DeviceObject,
                                           IrpSp->Parameters.StartDevice.AllocatedResources,
                                           IrpSp->Parameters.StartDevice.AllocatedResourcesTranslated);
            }
            break;

        case IRP_MN_QUERY_REMOVE_DEVICE:
            DPRINT1("  IRP_MN_QUERY_REMOVE_DEVICE\n");
            break;

        case IRP_MN_REMOVE_DEVICE:
            DPRINT1("  IRP_MN_REMOVE_DEVICE received\n");
            break;

        case IRP_MN_CANCEL_REMOVE_DEVICE:
            DPRINT1("  IRP_MN_CANCEL_REMOVE_DEVICE\n");
            break;

        case IRP_MN_STOP_DEVICE:
            DPRINT1("  IRP_MN_STOP_DEVICE received\n");
            break;

        case IRP_MN_QUERY_STOP_DEVICE:
            DPRINT1("  IRP_MN_QUERY_STOP_DEVICE received\n");
            break;

        case IRP_MN_CANCEL_STOP_DEVICE:
            DPRINT1("  IRP_MN_CANCEL_STOP_DEVICE\n");
            break;

        case IRP_MN_QUERY_DEVICE_RELATIONS:
            DPRINT1("  IRP_MN_QUERY_DEVICE_RELATIONS\n");

            switch (IrpSp->Parameters.QueryDeviceRelations.Type)
            {
                case BusRelations:
                    DPRINT1("    IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS / BusRelations\n");
                    Status = FdcFdoQueryBusRelations(DeviceObject, &DeviceRelations);
                    Information = (ULONG_PTR)DeviceRelations;
                    break;

                case RemovalRelations:
                    DPRINT1("    IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS / RemovalRelations\n");
                    return ForwardIrpAndForget(DeviceObject, Irp);

                default:
                    DPRINT1("    IRP_MJ_PNP / IRP_MN_QUERY_DEVICE_RELATIONS / Unknown type 0x%lx\n",
                            IrpSp->Parameters.QueryDeviceRelations.Type);
                    return ForwardIrpAndForget(DeviceObject, Irp);
            }
            break;

        case IRP_MN_SURPRISE_REMOVAL:
            DPRINT1("  IRP_MN_SURPRISE_REMOVAL received\n");
            break;

        default:
            DPRINT("  Unknown IOCTL 0x%lx\n", IrpSp->MinorFunction);
            return ForwardIrpAndForget(DeviceObject, Irp);
    }

    Irp->IoStatus.Information = Information;
    Irp->IoStatus.Status = Status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return Status;
}

/* EOF */