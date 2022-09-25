
#ifndef _GPIO_H_
#define _GPIO_H_

/*******************************************************************************
* Function Name  : CH559GPIODrivCap(UINT8 Port,UINT8 Cap)
* Description    : �˿�0��1��2��3������������
* Input          : UINT8 Port�˿�ѡ��(0��1��2��3)
                   UINT8 Cap��������ѡ��((0)5mA��(1)20mA(ע��:P1����10mA))
* Output         : None
* Return         : SUCCESS�ɹ�
                   FAILʧ��
*******************************************************************************/
extern UINT8 CH559GPIODrivCap(UINT8 Port,UINT8 Cap);


/*******************************************************************************
* Function Name  : CH559GPIOModeSelt(UINT8 Port,UINT8 Mode,UINT8 PinNum)
* Description    : �˿�0��1��2��3����ģʽ����
* Input          : UINT8 Port�˿�ѡ��(0��1��2��3)
                   UINT8 Cap������ʽѡ��(bPn_OC & Pn_DIR & Pn_PU)
                   0(000)�������룬��������
                   1(001)�������룬��������
                   2(01x)������������ߵ͵�ƽǿ������
                   3(100)����©�������������֧�����룻
                   4(110)����©�����������,��ת������ɵ͵���ʱ��������2��ʱ�ӵĸߵ�ƽ
                   5(101)��׼˫��(��׼51ģʽ)����©�����������
                   6(111)��׼˫��(��׼51ģʽ)����©���������������ת������ɵ͵���ʱ��������2��ʱ�ӵĸߵ�ƽ
                   UINT8 PinNum(����ѡ��0-7)
* Output         : None
* Return         : SUCCESS�ɹ�
                   FAILʧ��
*******************************************************************************/
extern UINT8 CH559GPIOModeSelt(UINT8 Port,UINT8 Mode,UINT8 PinNum);


/*******************************************************************************
* Function Name  : CH559P4Mode()
* Description    : CH559��P4�˿ڳ�ʼ����P4Ĭ���������
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
extern void CH559P4Mode();


/*******************************************************************************
* Function Name  : CH559GPIOInterruptInit()
* Description    : CH559GPIO�жϳ�ʼ��������������P5.5\P1.4\P0.3\P5.7\P4.1\RXD0����ͬ��
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
extern void CH559GPIOInterruptInit();

#endif

