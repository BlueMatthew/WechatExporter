#ifndef ExportNotifier_h
#define ExportNotifier_h

class ExportNotifier
{
public:

	virtual ~ExportNotifier() {}

	virtual bool isCancelled() const = 0;
    virtual void onStart() const = 0;
	virtual void onProgress(double progress) const = 0;
	virtual void onComplete(bool cancelled) const = 0;

};

#endif /* ExportNotifier_h */
