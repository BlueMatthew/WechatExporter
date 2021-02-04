#ifndef ExportNotifier_h
#define ExportNotifier_h

class ExportNotifier
{
public:

	virtual ~ExportNotifier() {}

    virtual void onStart() const = 0;
	virtual void onProgress(uint32_t numberOfMessages, uint32_t numberOfTotalMessages) const = 0;
	virtual void onComplete(bool cancelled) const = 0;

};

#endif /* ExportNotifier_h */
